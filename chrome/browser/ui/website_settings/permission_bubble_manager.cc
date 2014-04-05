// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PermissionBubbleManager);

namespace {

// This is how many ms to wait to see if there's another permission request
// we should coalesce.
const int kPermissionsCoalesceIntervalMs = 400;

}

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
    customization_mode_(false) {
  timer_.reset(new base::Timer(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kPermissionsCoalesceIntervalMs),
      base::Bind(&PermissionBubbleManager::ShowBubble, base::Unretained(this)),
      false));
}

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

  // Start the timer when there is both a view and a request.
  if (view_ && !timer_->IsRunning())
    timer_->Reset();
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

  // Even if there are requests queued up, add a short delay before the bubble
  // appears.
  if (!requests_.empty() && !timer_->IsRunning())
    timer_->Reset();
  else
    view_->Hide();
}

void PermissionBubbleManager::DidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    content::RenderViewHost* render_view_host) {
  // Allows extra time for additional requests to coalesce.
  if (timer_->IsRunning())
    timer_->Reset();
}

void PermissionBubbleManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // If the web contents has been destroyed, do not attempt to notify
  // the requests of any changes - simply close the bubble.
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
  if (view_ && !bubble_showing_ && requests_.size()) {
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
    // TODO(leng):  Explore other options of showing the next bubble.  The
    // advantage of this is that it uses the same code path as the first bubble.
    timer_->Reset();
  }
}

void PermissionBubbleManager::SetCoalesceIntervalForTesting(int interval_ms) {
  timer_.reset(new base::Timer(FROM_HERE,
      base::TimeDelta::FromMilliseconds(interval_ms),
      base::Bind(&PermissionBubbleManager::ShowBubble, base::Unretained(this)),
      false));
}
