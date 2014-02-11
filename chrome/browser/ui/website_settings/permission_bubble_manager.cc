// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PermissionBubbleManager);

// static
bool PermissionBubbleManager::Enabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePermissionsBubbles);
}

void PermissionBubbleManager::AddRequest(PermissionBubbleRequest* request) {
  // Don't re-add an existing request.
  std::vector<PermissionBubbleRequest*>::iterator di;
  for (di = requests_.begin(); di != requests_.end(); di++) {
    if (*di == request)
      return;
  }

  requests_.push_back(request);
  // TODO(gbillock): do we need to make default state a request property?
  accept_state_.push_back(true);

  // TODO(gbillock): need significantly more complex logic here to deal
  // with various states of the manager.

  if (view_ && !bubble_showing_) {
    view_->SetDelegate(this);
    view_->Show(requests_, accept_state_, customization_mode_);
    bubble_showing_ = true;
  }
}

void PermissionBubbleManager::SetView(PermissionBubbleView* view) {
  if (view == view_)
    return;

  if (view_ != NULL) {
    view_->SetDelegate(NULL);
    view_->Hide();
  }

  view_ = view;
  if (view_)
    view_->SetDelegate(this);
  else
    return;

  if (!requests_.empty())
    bubble_showing_ = true;

  if (bubble_showing_)
    view_->Show(requests_, accept_state_, customization_mode_);
  else
    view_->Hide();
}

PermissionBubbleManager::PermissionBubbleManager(
    content::WebContents* web_contents)
  : content::WebContentsObserver(web_contents),
    bubble_showing_(false),
    view_(NULL),
    customization_mode_(false) {
}

PermissionBubbleManager::~PermissionBubbleManager() {}

void PermissionBubbleManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // Synthetic cancel event if the user closes the WebContents.
  Closing();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission bubbles
  // or use us otherwise during the destruction.
  web_contents->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionBubbleManager::ToggleAccept(int request_index, bool new_value) {
  DCHECK(request_index < static_cast<int>(accept_state_.size()));
  accept_state_[request_index] = new_value;
}

void PermissionBubbleManager::SetCustomizationMode() {
  customization_mode_ = true;
  if (view_)
    view_->Show(requests_, accept_state_, customization_mode_);
}

void PermissionBubbleManager::Accept() {
  std::vector<PermissionBubbleRequest*>::iterator di;
  std::vector<bool>::iterator ai;
  for (di = requests_.begin(), ai = accept_state_.begin();
       di != requests_.end(); di++, ai++) {
    if (*ai)
      (*di)->PermissionGranted();
    else
      (*di)->PermissionDenied();
  }
  FinalizeBubble();
}

void PermissionBubbleManager::Deny() {
  std::vector<PermissionBubbleRequest*>::iterator di;
  for (di = requests_.begin(); di != requests_.end(); di++)
    (*di)->PermissionDenied();
  FinalizeBubble();
}

void PermissionBubbleManager::Closing() {
  std::vector<PermissionBubbleRequest*>::iterator di;
  for (di = requests_.begin(); di != requests_.end(); di++)
    (*di)->Cancelled();
  FinalizeBubble();
}

void PermissionBubbleManager::FinalizeBubble() {
  if (view_) {
    view_->SetDelegate(NULL);
    view_->Hide();
  }

  std::vector<PermissionBubbleRequest*>::iterator di;
  for (di = requests_.begin(); di != requests_.end(); di++)
    (*di)->RequestFinished();
  requests_.clear();
  accept_state_.clear();
}

