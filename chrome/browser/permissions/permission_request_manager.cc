// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "url/origin.h"

namespace {

class CancelledRequest : public PermissionRequest {
 public:
  explicit CancelledRequest(PermissionRequest* cancelled)
      : icon_(cancelled->GetIconId()),
        message_fragment_(cancelled->GetMessageTextFragment()),
        origin_(cancelled->GetOrigin()) {}
  ~CancelledRequest() override {}

  IconId GetIconId() const override { return icon_; }
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
  IconId icon_;
  base::string16 message_fragment_;
  GURL origin_;
};

bool IsMessageTextEqual(PermissionRequest* a,
                        PermissionRequest* b) {
  if (a == b)
    return true;
  if (a->GetMessageTextFragment() == b->GetMessageTextFragment() &&
      a->GetOrigin() == b->GetOrigin()) {
    return true;
  }
  return false;
}

}  // namespace

// PermissionRequestManager::Observer ------------------------------------------

PermissionRequestManager::Observer::~Observer() {
}

void PermissionRequestManager::Observer::OnBubbleAdded() {
}

// PermissionRequestManager ----------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PermissionRequestManager);

PermissionRequestManager::PermissionRequestManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      view_factory_(base::Bind(&PermissionPrompt::Create)),
      view_(nullptr),
      main_frame_has_fully_loaded_(false),
      persist_(true),
      auto_response_for_test_(NONE),
      weak_factory_(this) {
#if defined(OS_ANDROID)
  view_ = view_factory_.Run(web_contents);
  view_->SetDelegate(this);
#endif
}

PermissionRequestManager::~PermissionRequestManager() {
  if (view_ != NULL)
    view_->SetDelegate(NULL);

  for (PermissionRequest* request : requests_)
    request->RequestFinished();
  for (PermissionRequest* request : queued_requests_)
    request->RequestFinished();
  for (PermissionRequest* request : queued_frame_requests_)
    request->RequestFinished();
  for (const auto& entry : duplicate_requests_)
    entry.second->RequestFinished();
}

void PermissionRequestManager::AddRequest(PermissionRequest* request) {
  // TODO(tsergeant): change the UMA to no longer mention bubbles.
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));

  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  const GURL& request_url_ = web_contents()->GetLastCommittedURL();
  bool is_main_frame = url::Origin(request_url_)
                           .IsSameOriginWith(url::Origin(request->GetOrigin()));

  // Don't re-add an existing request or one with a duplicate text request.
  PermissionRequest* existing_request = GetExistingRequest(request);
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

  if (is_main_frame) {
    if (IsBubbleVisible()) {
      base::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
    }
    queued_requests_.push_back(request);
  } else {
    base::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
    queued_frame_requests_.push_back(request);
  }

  if (!IsBubbleVisible())
    ScheduleShowBubble();
}

void PermissionRequestManager::CancelRequest(PermissionRequest* request) {
  // First look in the queued requests, where we can simply finish the request
  // and go on.
  std::vector<PermissionRequest*>::iterator requests_iter;
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
    bool can_erase = !view_ || view_->CanAcceptRequestUpdate();
    if (can_erase) {
      RequestFinishedIncludingDuplicates(*requests_iter);
      requests_.erase(requests_iter);
      accept_states_.erase(accepts_iter);

      if (view_) {
        view_->Hide();
        // Will redraw the bubble if it is being shown.
        DequeueRequestsAndShowBubble();
      }
      return;
    }

    // Cancel the existing request and replace it with a dummy.
    PermissionRequest* cancelled_request =
        new CancelledRequest(*requests_iter);
    RequestFinishedIncludingDuplicates(*requests_iter);
    *requests_iter = cancelled_request;
    return;
  }

  // Since |request| wasn't found in queued_requests_, queued_frame_requests_ or
  // requests_ it must have been marked as a duplicate. We can't search
  // duplicate_requests_ by value, so instead use GetExistingRequest to find the
  // key (request it was duped against), and iterate through duplicates of that.
  PermissionRequest* existing_request = GetExistingRequest(request);
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

void PermissionRequestManager::HideBubble() {
  // Disengage from the existing view if there is one and it doesn't manage
  // its own visibility.
  if (!view_ || view_->HidesAutomatically())
    return;

  view_->SetDelegate(nullptr);
  view_->Hide();
  view_.reset();
}

void PermissionRequestManager::DisplayPendingRequests() {
  if (IsBubbleVisible())
    return;

  view_ = view_factory_.Run(web_contents());
  view_->SetDelegate(this);

  if (!main_frame_has_fully_loaded_)
    return;

  if (requests_.empty()) {
    DequeueRequestsAndShowBubble();
  } else {
    // We switched tabs away and back while a prompt was active.
    ShowBubble();
  }
}

void PermissionRequestManager::UpdateAnchorPosition() {
  if (view_)
    view_->UpdateAnchorPosition();
}

bool PermissionRequestManager::IsBubbleVisible() {
  return view_ && !requests_.empty();
}

// static
bool PermissionRequestManager::IsEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(features::kUseGroupedPermissionInfobars);
#else
  return true;
#endif
}

gfx::NativeWindow PermissionRequestManager::GetBubbleWindow() {
  if (view_)
    return view_->GetNativeWindow();
  return nullptr;
}

void PermissionRequestManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  CancelPendingQueues();
  FinalizeBubble();
  main_frame_has_fully_loaded_ = false;
}

void PermissionRequestManager::DocumentOnLoadCompletedInMainFrame() {
  main_frame_has_fully_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleShowBubble();
}

void PermissionRequestManager::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  ScheduleShowBubble();
}

void PermissionRequestManager::WebContentsDestroyed() {
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

const std::vector<PermissionRequest*>& PermissionRequestManager::Requests() {
  return requests_;
}

const std::vector<bool>& PermissionRequestManager::AcceptStates() {
  return accept_states_;
}

void PermissionRequestManager::ToggleAccept(int request_index, bool new_value) {
  DCHECK(request_index < static_cast<int>(accept_states_.size()));
  accept_states_[request_index] = new_value;
}

void PermissionRequestManager::TogglePersist(bool new_value) {
  persist_ = new_value;
}

void PermissionRequestManager::Accept() {
  PermissionUmaUtil::PermissionPromptAccepted(requests_, accept_states_);

  std::vector<PermissionRequest*>::iterator requests_iter;
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

void PermissionRequestManager::Deny() {
  DCHECK_EQ(1u, requests_.size());
  PermissionUmaUtil::PermissionPromptDenied(requests_);

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble();
}

void PermissionRequestManager::Closing() {
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble();
}

void PermissionRequestManager::ScheduleShowBubble() {
  // ::ScheduleShowBubble() will be called again when the main frame will be
  // loaded.
  if (!main_frame_has_fully_loaded_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PermissionRequestManager::DequeueRequestsAndShowBubble,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::DequeueRequestsAndShowBubble() {
  if (!view_)
    return;
  if (!requests_.empty())
    return;
  if (!main_frame_has_fully_loaded_)
    return;
  if (queued_requests_.empty() && queued_frame_requests_.empty())
    return;

  if (queued_requests_.size())
    requests_.swap(queued_requests_);
  else
    requests_.swap(queued_frame_requests_);

  // Sets the default value for each request to be 'accept'.
  accept_states_.resize(requests_.size(), true);

  ShowBubble();
}

void PermissionRequestManager::ShowBubble() {
  DCHECK(view_);
  DCHECK(!requests_.empty());
  DCHECK(main_frame_has_fully_loaded_);

  view_->Show();
  PermissionUmaUtil::PermissionPromptShown(requests_);
  NotifyBubbleAdded();

  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE)
    DoAutoResponseForTesting();
}

void PermissionRequestManager::FinalizeBubble() {
  if (view_ && !view_->HidesAutomatically())
    view_->Hide();

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  requests_.clear();
  accept_states_.clear();
  if (queued_requests_.size() || queued_frame_requests_.size())
    DequeueRequestsAndShowBubble();
}

void PermissionRequestManager::CancelPendingQueues() {
  std::vector<PermissionRequest*>::iterator requests_iter;
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

PermissionRequest* PermissionRequestManager::GetExistingRequest(
    PermissionRequest* request) {
  for (PermissionRequest* existing_request : requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  for (PermissionRequest* existing_request : queued_requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  for (PermissionRequest* existing_request : queued_frame_requests_)
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  return nullptr;
}

void PermissionRequestManager::PermissionGrantedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->set_persist(persist_);
  request->PermissionGranted();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it) {
    it->second->set_persist(persist_);
    it->second->PermissionGranted();
  }
}
void PermissionRequestManager::PermissionDeniedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->set_persist(persist_);
  request->PermissionDenied();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it) {
    it->second->set_persist(persist_);
    it->second->PermissionDenied();
  }
}
void PermissionRequestManager::CancelledIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->Cancelled();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->Cancelled();
}
void PermissionRequestManager::RequestFinishedIncludingDuplicates(
    PermissionRequest* request) {
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

void PermissionRequestManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionRequestManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PermissionRequestManager::NotifyBubbleAdded() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleAdded();
}

void PermissionRequestManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      // Deny() assumes there is only 1 request.
      if (accept_states_.size() == 1) {
        Deny();
      } else {
        for (size_t i = 0; i < accept_states_.size(); ++i)
          accept_states_[i] = false;
        Accept();
      }
      break;
    case DISMISS:
      Closing();
      break;
    case NONE:
      NOTREACHED();
  }
}
