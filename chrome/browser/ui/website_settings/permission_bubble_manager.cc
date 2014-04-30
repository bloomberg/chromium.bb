// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

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
  // TODO(gbillock): implement
  NOTREACHED();
}

void PermissionBubbleManager::SetView(PermissionBubbleView* view) {
  if (view == view_)
    return;

  if (view_ != NULL) {
    view_->SetDelegate(NULL);
    view_->Hide();
    bubble_showing_ = false;
  }

  view_ = view;
  if (view_)
    view_->SetDelegate(this);
  else
    return;

  ShowBubble();
}

void PermissionBubbleManager::DocumentOnLoadCompletedInMainFrame(
    int32 page_id) {
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
  if (!request_url_.is_empty() &&
      request_url_ != web_contents()->GetLastCommittedURL()) {
    // Kill off existing bubble and cancel any pending requests.
    CancelPendingQueue();
    FinalizeBubble();
  }
}

void PermissionBubbleManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // If the web contents has been destroyed, do not attempt to notify
  // the requests of any changes - simply close the bubble.
  CancelPendingQueue();
  FinalizeBubble();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission bubbles
  // or use us otherwise during the destruction.
  web_contents->RemoveUserData(UserDataKey());
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

void PermissionBubbleManager::ShowBubble() {
  if (view_ && !bubble_showing_ && request_url_has_loaded_ &&
      requests_.size()) {
    // Note: this should appear above Show() for testing, since in that
    // case we may do in-line calling of finalization.
    bubble_showing_ = true;
    view_->Show(requests_, accept_states_, customization_mode_);
  }
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
