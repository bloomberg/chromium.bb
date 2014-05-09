// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
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
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePermissionsBubbles);
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

  // Don't re-add an existing request or one with a duplicate text request.
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    if (*requests_iter == request)
      return;
    // TODO(gbillock): worry about the requesting host name as well.
    if ((*requests_iter)->GetMessageTextFragment() ==
        request->GetMessageTextFragment()) {
      request->RequestFinished();
      return;
    }
  }
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    if (*requests_iter == request)
      return;
    if ((*requests_iter)->GetMessageTextFragment() ==
        request->GetMessageTextFragment()) {
      request->RequestFinished();
      return;
    }
  }

  if (bubble_showing_) {
    content::RecordAction(
        base::UserMetricsAction("PermissionBubbleRequestQueued"));
    queued_requests_.push_back(request);
    return;
  }

  requests_.push_back(request);
  // TODO(gbillock): do we need to make default state a request property?
  accept_states_.push_back(true);

  if (request->HasUserGesture())
    ShowBubble();
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
      ShowBubble();  // Will redraw the bubble if it is being shown.
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
  ShowBubble();
}

void PermissionBubbleManager::DocumentOnLoadCompletedInMainFrame() {
  request_url_has_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  // TODO(gbillock): make this bind safe with a weak ptr.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&PermissionBubbleManager::ShowBubble,
                 weak_factory_.GetWeakPtr()));
}

void PermissionBubbleManager::NavigationEntryCommitted(
    const content::LoadCommittedDetails& details) {
  // No permissions requests pending.
  if (request_url_.is_empty())
    return;

  // If we have navigated to a new url...
  if (request_url_ != web_contents()->GetLastCommittedURL()) {
    // Kill off existing bubble and cancel any pending requests.
    CancelPendingQueue();
    FinalizeBubble();
  }
}

void PermissionBubbleManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the bubble as cancelled.
  CancelPendingQueue();
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

// TODO(gbillock): find a better name for this method.
void PermissionBubbleManager::ShowBubble() {
  if (!view_)
    return;
  if (requests_.empty())
    return;
  if (bubble_showing_)
    return;
  if (!request_url_has_loaded_)
    return;

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
  if (queued_requests_.size()) {
    requests_ = queued_requests_;
    accept_states_.resize(requests_.size(), true);
    queued_requests_.clear();
    ShowBubble();
  } else {
    request_url_ = GURL();
  }
}

void PermissionBubbleManager::CancelPendingQueue() {
  std::vector<PermissionBubbleRequest*>::iterator requests_iter;
  for (requests_iter = queued_requests_.begin();
       requests_iter != queued_requests_.end();
       requests_iter++) {
    (*requests_iter)->RequestFinished();
  }
}
