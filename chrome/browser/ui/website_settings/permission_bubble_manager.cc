// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/user_metrics.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace {

class CancelledRequest : public PermissionBubbleRequest {
 public:
  explicit CancelledRequest(PermissionBubbleRequest* cancelled)
      : icon_(cancelled->GetIconId()),
        message_text_(cancelled->GetMessageText()),
        message_fragment_(cancelled->GetMessageTextFragment()),
        origin_(cancelled->GetOrigin()) {}
  ~CancelledRequest() override {}

  int GetIconId() const override { return icon_; }
  base::string16 GetMessageText() const override { return message_text_; }
  base::string16 GetMessageTextFragment() const override {
    return message_fragment_;
  }
  GURL GetOrigin() const override { return origin_; }

  // These are all no-ops since the placeholder is non-forwarding.
  void PermissionGranted() override {}
  void PermissionDenied() override {}
  void Cancelled() override {}

  void RequestFinished() override { delete this; }

 private:
  int icon_;
  base::string16 message_text_;
  base::string16 message_fragment_;
  GURL origin_;
};

bool IsMessageTextEqual(PermissionBubbleRequest* a,
                        PermissionBubbleRequest* b) {
  if (a == b)
    return true;
  if (a->GetMessageTextFragment() == b->GetMessageTextFragment() &&
      a->GetOrigin() == b->GetOrigin()) {
    return true;
  }
  return false;
}

}  // namespace

// PermissionBubbleManager::Observer -------------------------------------------

PermissionBubbleManager::Observer::~Observer() {
}

void PermissionBubbleManager::Observer::OnBubbleAdded() {
}

// PermissionBubbleManager -----------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PermissionBubbleManager);

PermissionBubbleManager::PermissionBubbleManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
#if !defined(OS_ANDROID)  // No bubbles in android tests.
      view_factory_(base::Bind(&PermissionBubbleView::Create)),
#endif
      view_(nullptr),
      main_frame_has_fully_loaded_(false),
      auto_response_for_test_(NONE),
      weak_factory_(this) {
}

PermissionBubbleManager::~PermissionBubbleManager() {
  if (view_ != NULL)
    view_->SetDelegate(NULL);

  for (PermissionBubbleRequest* request : requests_)
    request->RequestFinished();
  for (PermissionBubbleRequest* request : queued_requests_)
    request->RequestFinished();
  for (PermissionBubbleRequest* request : queued_frame_requests_)
    request->RequestFinished();
  for (const auto& entry : duplicate_requests_)
    entry.second->RequestFinished();
}

void PermissionBubbleManager::AddRequest(PermissionBubbleRequest* request) {
  content::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));
  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  request_url_ = web_contents()->GetLastCommittedURL();
  bool is_main_frame = url::Origin(request_url_)
                           .IsSameOriginWith(url::Origin(request->GetOrigin()));

  // Don't re-add an existing request or one with a duplicate text request.
  PermissionBubbleRequest* existing_request = GetExistingRequest(request);
  if (existing_request) {
    // |request| is a duplicate. Add it to |duplicate_requests_| unless it's the
    // same object as |existing_request| or an existing duplicate.
    if (request == existing_request)
      return;
    auto range = duplicate_requests_.equal_range(existing_request);
    for (auto it = range.first; it != range.second; ++it) {
      if (request == it->second)
        return;
    }
    duplicate_requests_.insert(std::make_pair(existing_request, request));
    return;
  }

  if (IsBubbleVisible()) {
    if (is_main_frame) {
      content::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
      queued_requests_.push_back(request);
    } else {
      content::RecordAction(
          base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
      queued_frame_requests_.push_back(request);
    }
    return;
  }

  if (is_main_frame) {
    requests_.push_back(request);
    // TODO(gbillock): do we need to make default state a request property?
    accept_states_.push_back(true);
  } else {
    content::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
    queued_frame_requests_.push_back(request);
  }

  ScheduleShowBubble();
}

void PermissionBubbleManager::CancelRequest(PermissionBubbleRequest* request) {
  // First look in the queued requests, where we can simply finish the request
  // and go on.
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    if (*requests_iter == request) {
      RequestFinishedIncludingDuplicates(*requests_iter);
      queued_requests_.erase(requests_iter);
      return;
    }
  }
  for (requests_iter = queued_frame_requests_.begin();
       requests_iter != queued_frame_requests_.end(); requests_iter++) {
    if (*requests_iter == request) {
      RequestFinishedIncludingDuplicates(*requests_iter);
      queued_frame_requests_.erase(requests_iter);
      return;
    }
  }

  std::vector<bool>::iterator accepts_iter = accept_states_.begin();
  for (requests_iter = requests_.begin(), accepts_iter = accept_states_.begin();
       requests_iter != requests_.end();
       requests_iter++, accepts_iter++) {
    if (*requests_iter != request)
      continue;

    // We can simply erase the current entry in the request table if we aren't
    // showing the dialog, or if we are showing it and it can accept the update.
    bool can_erase = !IsBubbleVisible() || view_->CanAcceptRequestUpdate();
    if (can_erase) {
      RequestFinishedIncludingDuplicates(*requests_iter);
      requests_.erase(requests_iter);
      accept_states_.erase(accepts_iter);

      if (IsBubbleVisible()) {
        view_->Hide();
        // Will redraw the bubble if it is being shown.
        TriggerShowBubble();
      }
      return;
    }

    // Cancel the existing request and replace it with a dummy.
    PermissionBubbleRequest* cancelled_request =
        new CancelledRequest(*requests_iter);
    RequestFinishedIncludingDuplicates(*requests_iter);
    *requests_iter = cancelled_request;
    return;
  }

  // Since |request| wasn't found in queued_requests_, queued_frame_requests_ or
  // requests_ it must have been marked as a duplicate. We can't search
  // duplicate_requests_ by value, so instead use GetExistingRequest to find the
  // key (request it was duped against), and iterate through duplicates of that.
  PermissionBubbleRequest* existing_request = GetExistingRequest(request);
  auto range = duplicate_requests_.equal_range(existing_request);
  for (auto it = range.first; it != range.second; ++it) {
    if (request == it->second) {
      it->second->RequestFinished();
      duplicate_requests_.erase(it);
      return;
    }
  }

  NOTREACHED();  // Callers should not cancel requests that are not pending.
}

void PermissionBubbleManager::HideBubble() {
  // Disengage from the existing view if there is one.
  if (!view_)
    return;

  view_->SetDelegate(nullptr);
  view_->Hide();
  view_.reset();
}

void PermissionBubbleManager::DisplayPendingRequests() {
  if (IsBubbleVisible())
    return;

#if defined(OS_ANDROID)
  NOTREACHED();
  return;
#else
  view_ = view_factory_.Run(chrome::FindBrowserWithWebContents(web_contents()));
  view_->SetDelegate(this);
#endif

  TriggerShowBubble();
}

void PermissionBubbleManager::UpdateAnchorPosition() {
  if (view_)
    view_->UpdateAnchorPosition();
}

bool PermissionBubbleManager::IsBubbleVisible() {
  return view_ && view_->IsVisible();
}

gfx::NativeWindow PermissionBubbleManager::GetBubbleWindow() {
  if (view_)
    return view_->GetNativeWindow();
  return nullptr;
}

void PermissionBubbleManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  CancelPendingQueues();
  FinalizeBubble();
  main_frame_has_fully_loaded_ = false;
}

void PermissionBubbleManager::DocumentOnLoadCompletedInMainFrame() {
  main_frame_has_fully_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleShowBubble();
}

void PermissionBubbleManager::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  ScheduleShowBubble();
}

void PermissionBubbleManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the bubble as cancelled.
  CancelPendingQueues();
  FinalizeBubble();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission bubbles
  // or use us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionBubbleManager::ToggleAccept(int request_index, bool new_value) {
  DCHECK(request_index < static_cast<int>(accept_states_.size()));
  accept_states_[request_index] = new_value;
}

void PermissionBubbleManager::Accept() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  std::vector<bool>::iterator accepts_iter = accept_states_.begin();
  for (requests_iter = requests_.begin(), accepts_iter = accept_states_.begin();
       requests_iter != requests_.end();
       requests_iter++, accepts_iter++) {
    if (*accepts_iter) {
      PermissionGrantedIncludingDuplicates(*requests_iter);
    } else {
      PermissionDeniedIncludingDuplicates(*requests_iter);
    }
  }
  FinalizeBubble();
}

void PermissionBubbleManager::Deny() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble();
}

void PermissionBubbleManager::Closing() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble();
}

void PermissionBubbleManager::ScheduleShowBubble() {
  // ::ScheduleShowBubble() will be called again when the main frame will be
  // loaded.
  if (!main_frame_has_fully_loaded_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PermissionBubbleManager::TriggerShowBubble,
                 weak_factory_.GetWeakPtr()));
}

void PermissionBubbleManager::TriggerShowBubble() {
  if (!view_)
    return;
  if (IsBubbleVisible())
    return;
  if (!main_frame_has_fully_loaded_)
    return;
  if (requests_.empty() && queued_requests_.empty() &&
      queued_frame_requests_.empty()) {
    return;
  }

  if (requests_.empty()) {
    if (queued_requests_.size())
      requests_.swap(queued_requests_);
    else
      requests_.swap(queued_frame_requests_);

    // Sets the default value for each request to be 'accept'.
    // TODO(leng):  Currently all requests default to true.  If that changes:
    // a) Add additional accept_state queues to store default values.
    // b) Change the request API to provide the default value.
    accept_states_.resize(requests_.size(), true);
  }

  view_->Show(requests_, accept_states_);
  PermissionUmaUtil::PermissionPromptShown(requests_);
  NotifyBubbleAdded();

  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE)
    DoAutoResponseForTesting();
}

void PermissionBubbleManager::FinalizeBubble() {
  if (view_)
    view_->Hide();

  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  requests_.clear();
  accept_states_.clear();
  if (queued_requests_.size() || queued_frame_requests_.size())
    TriggerShowBubble();
  else
    request_url_ = GURL();
}

void PermissionBubbleManager::CancelPendingQueues() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  for (requests_iter = queued_frame_requests_.begin();
       requests_iter != queued_frame_requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  queued_requests_.clear();
  queued_frame_requests_.clear();
}

PermissionBubbleRequest* PermissionBubbleManager::GetExistingRequest(
    PermissionBubbleRequest* request) {
  for (PermissionBubbleRequest* existing_request : requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  for (PermissionBubbleRequest* existing_request : queued_requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  for (PermissionBubbleRequest* existing_request : queued_frame_requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  return nullptr;
}

void PermissionBubbleManager::PermissionGrantedIncludingDuplicates(
    PermissionBubbleRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->PermissionGranted();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionGranted();
}
void PermissionBubbleManager::PermissionDeniedIncludingDuplicates(
    PermissionBubbleRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->PermissionDenied();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionDenied();
}
void PermissionBubbleManager::CancelledIncludingDuplicates(
    PermissionBubbleRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->Cancelled();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->Cancelled();
}
void PermissionBubbleManager::RequestFinishedIncludingDuplicates(
    PermissionBubbleRequest* request) {
  // We can't call GetExistingRequest here, because other entries in requests_,
  // queued_requests_ or queued_frame_requests_ might already have been deleted.
  DCHECK_EQ(1, std::count(requests_.begin(), requests_.end(), request) +
               std::count(queued_requests_.begin(), queued_requests_.end(),
                          request) +
               std::count(queued_frame_requests_.begin(),
                          queued_frame_requests_.end(), request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->RequestFinished();
  // Beyond this point, |request| has probably been deleted.
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->RequestFinished();
  // Additionally, we can now remove the duplicates.
  duplicate_requests_.erase(request);
}

void PermissionBubbleManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionBubbleManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PermissionBubbleManager::NotifyBubbleAdded() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnBubbleAdded());
}

void PermissionBubbleManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      Deny();
      break;
    case DISMISS:
      Closing();
      break;
    case NONE:
      NOTREACHED();
  }
}
