// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/user_metrics.h"

namespace {

class CancelledRequest : public PermissionBubbleRequest {
 public:
  explicit CancelledRequest(PermissionBubbleRequest* cancelled)
      : icon_(cancelled->GetIconID()),
        message_text_(cancelled->GetMessageText()),
        message_fragment_(cancelled->GetMessageTextFragment()),
        user_gesture_(cancelled->HasUserGesture()),
        hostname_(cancelled->GetRequestingHostname()) {}
  virtual ~CancelledRequest() {}

  virtual int GetIconID() const OVERRIDE {
    return icon_;
  }
  virtual base::string16 GetMessageText() const OVERRIDE {
    return message_text_;
  }
  virtual base::string16 GetMessageTextFragment() const OVERRIDE {
    return message_fragment_;
  }
  virtual bool HasUserGesture() const OVERRIDE {
    return user_gesture_;
  }
  virtual GURL GetRequestingHostname() const OVERRIDE {
    return hostname_;
  }

  // These are all no-ops since the placeholder is non-forwarding.
  virtual void PermissionGranted() OVERRIDE {}
  virtual void PermissionDenied() OVERRIDE {}
  virtual void Cancelled() OVERRIDE {}

  virtual void RequestFinished() OVERRIDE {
    delete this;
  }

 private:
  int icon_;
  base::string16 message_text_;
  base::string16 message_fragment_;
  bool user_gesture_;
  GURL hostname_;
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PermissionBubbleManager);

// static
bool PermissionBubbleManager::Enabled() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePermissionsBubbles))
    return true;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePermissionsBubbles))
    return false;

  return false;
}

PermissionBubbleManager::PermissionBubbleManager(
    content::WebContents* web_contents)
  : content::WebContentsObserver(web_contents),
    bubble_showing_(false),
    view_(NULL),
    request_url_has_loaded_(false),
    customization_mode_(false),
    weak_factory_(this) {}

PermissionBubbleManager::~PermissionBubbleManager() {
  if (view_ != NULL)
    view_->SetDelegate(NULL);

  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    (*requests_iter)->RequestFinished();
  }
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    (*requests_iter)->RequestFinished();
  }
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
  bool is_main_frame =
      request->GetRequestingHostname().GetOrigin() == request_url_.GetOrigin();

  // Don't re-add an existing request or one with a duplicate text request.
  bool same_object = false;
  if (ExistingRequest(request, requests_, &same_object) ||
      ExistingRequest(request, queued_requests_, &same_object) ||
      ExistingRequest(request, queued_frame_requests_, &same_object)) {
    if (!same_object)
      request->RequestFinished();
    return;
  }

  if (bubble_showing_) {
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

  if (request->HasUserGesture())
    ScheduleShowBubble();
}

void PermissionBubbleManager::CancelRequest(PermissionBubbleRequest* request) {
  // First look in the queued requests, where we can simply delete the request
  // and go on.
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    if (*requests_iter == request) {
      (*requests_iter)->RequestFinished();
      queued_requests_.erase(requests_iter);
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
    bool can_erase = !bubble_showing_ ||
                     !view_ || view_->CanAcceptRequestUpdate();
    if (can_erase) {
      (*requests_iter)->RequestFinished();
      requests_.erase(requests_iter);
      accept_states_.erase(accepts_iter);
      TriggerShowBubble();  // Will redraw the bubble if it is being shown.
      return;
    }

    // Cancel the existing request and replace it with a dummy.
    PermissionBubbleRequest* cancelled_request =
        new CancelledRequest(*requests_iter);
    (*requests_iter)->RequestFinished();
    *requests_iter = cancelled_request;
    return;
  }

  NOTREACHED();  // Callers should not cancel requests that are not pending.
}

void PermissionBubbleManager::SetView(PermissionBubbleView* view) {
  if (view == view_)
    return;

  // Disengage from the existing view if there is one.
  if (view_ != NULL) {
    view_->SetDelegate(NULL);
    view_->Hide();
    bubble_showing_ = false;
  }

  view_ = view;
  if (!view)
    return;

  view->SetDelegate(this);
  TriggerShowBubble();
}

void PermissionBubbleManager::DocumentOnLoadCompletedInMainFrame() {
  request_url_has_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleShowBubble();
}

void PermissionBubbleManager::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (request_url_has_loaded_)
    ScheduleShowBubble();
}

void PermissionBubbleManager::NavigationEntryCommitted(
    const content::LoadCommittedDetails& details) {
  // No permissions requests pending.
  if (request_url_.is_empty())
    return;

  // If we have navigated to a new url or reloaded the page...
  // GetAsReferrer strips fragment and username/password, meaning
  // the navigation is really to the same page.
  if ((request_url_.GetAsReferrer() !=
       web_contents()->GetLastCommittedURL().GetAsReferrer()) ||
      details.type == content::NAVIGATION_TYPE_EXISTING_PAGE) {
    // Kill off existing bubble and cancel any pending requests.
    CancelPendingQueues();
    FinalizeBubble();
  }
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

void PermissionBubbleManager::SetCustomizationMode() {
  customization_mode_ = true;
  if (view_)
    view_->Show(requests_, accept_states_, customization_mode_);
}

void PermissionBubbleManager::Accept() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  std::vector<bool>::iterator accepts_iter = accept_states_.begin();
  for (requests_iter = requests_.begin(), accepts_iter = accept_states_.begin();
       requests_iter != requests_.end();
       requests_iter++, accepts_iter++) {
    if (*accepts_iter)
      (*requests_iter)->PermissionGranted();
    else
      (*requests_iter)->PermissionDenied();
  }
  FinalizeBubble();
}

void PermissionBubbleManager::Deny() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    (*requests_iter)->PermissionDenied();
  }
  FinalizeBubble();
}

void PermissionBubbleManager::Closing() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    (*requests_iter)->Cancelled();
  }
  FinalizeBubble();
}

void PermissionBubbleManager::ScheduleShowBubble() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PermissionBubbleManager::TriggerShowBubble,
                 weak_factory_.GetWeakPtr()));
}

void PermissionBubbleManager::TriggerShowBubble() {
  if (!view_)
    return;
  if (bubble_showing_)
    return;
  if (!request_url_has_loaded_)
    return;
  if (requests_.empty() && queued_requests_.empty() &&
      queued_frame_requests_.empty()) {
    return;
  }

  if (requests_.empty()) {
    // Queues containing a user-gesture-generated request have priority.
    if (HasUserGestureRequest(queued_requests_))
      requests_.swap(queued_requests_);
    else if (HasUserGestureRequest(queued_frame_requests_))
      requests_.swap(queued_frame_requests_);
    else if (queued_requests_.size())
      requests_.swap(queued_requests_);
    else
      requests_.swap(queued_frame_requests_);

    // Sets the default value for each request to be 'accept'.
    // TODO(leng):  Currently all requests default to true.  If that changes:
    // a) Add additional accept_state queues to store default values.
    // b) Change the request API to provide the default value.
    accept_states_.resize(requests_.size(), true);
  }

  // Note: this should appear above Show() for testing, since in that
  // case we may do in-line calling of finalization.
  bubble_showing_ = true;
  view_->Show(requests_, accept_states_, customization_mode_);
}

void PermissionBubbleManager::FinalizeBubble() {
  if (view_)
    view_->Hide();
  bubble_showing_ = false;

  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    (*requests_iter)->RequestFinished();
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
    (*requests_iter)->RequestFinished();
  }
  for (requests_iter = queued_frame_requests_.begin();
       requests_iter != queued_frame_requests_.end();
       requests_iter++) {
    (*requests_iter)->RequestFinished();
  }
  queued_requests_.clear();
  queued_frame_requests_.clear();
}

bool PermissionBubbleManager::ExistingRequest(
    PermissionBubbleRequest* request,
    const std::vector<PermissionBubbleRequest*>& queue,
    bool* same_object) {
  CHECK(same_object);
  *same_object = false;
  std::vector<PermissionBubbleRequest*>::const_iterator iter;
  for (iter = queue.begin(); iter != queue.end(); iter++) {
    if (*iter == request) {
      *same_object = true;
      return true;
    }
    if ((*iter)->GetMessageTextFragment() ==
            request->GetMessageTextFragment() &&
        (*iter)->GetRequestingHostname() == request->GetRequestingHostname()) {
      return true;
    }
  }
  return false;
}

bool PermissionBubbleManager::HasUserGestureRequest(
    const std::vector<PermissionBubbleRequest*>& queue) {
  std::vector<PermissionBubbleRequest*>::const_iterator iter;
  for (iter = queue.begin(); iter != queue.end(); iter++) {
    if ((*iter)->HasUserGesture())
      return true;
  }
  return false;
}

