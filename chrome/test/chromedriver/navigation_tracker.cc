// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/navigation_tracker.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client.h"
#include "chrome/test/chromedriver/status.h"

NavigationTracker::NavigationTracker(DevToolsClient* client) : client_(client) {
  DCHECK(client_);
  client_->AddListener(this);
}

NavigationTracker::~NavigationTracker() {}

bool NavigationTracker::IsPendingNavigation(const std::string& frame_id) {
  return frame_state_[frame_id].IsPendingNavigation();
}

Status NavigationTracker::OnConnected() {
  frame_state_.clear();
  // Enable page domain notifications to allow tracking navigation state.
  base::DictionaryValue params;
  return client_->SendCommand("Page.enable", params);
}

void NavigationTracker::OnEvent(const std::string& method,
                                const base::DictionaryValue& params) {
  if (method == "Page.frameStartedLoading" ||
      method == "Page.frameStoppedLoading" ||
      method == "Page.frameScheduledNavigation" ||
      method == "Page.frameClearedScheduledNavigation") {
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id)) {
      LOG(ERROR) << StringPrintf("%s missing 'frameId'", method.c_str());
      return;
    }
    if (method == "Page.frameStartedLoading") {
      frame_state_[frame_id].SetFlags(NavigationState::LOADING);
    } else if (method == "Page.frameStoppedLoading") {
      frame_state_[frame_id].ClearFlags(NavigationState::LOADING);
    } else if (method == "Page.frameScheduledNavigation") {
      frame_state_[frame_id].SetFlags(NavigationState::SCHEDULED);
    } else if (method == "Page.frameClearedScheduledNavigation") {
      frame_state_[frame_id].ClearFlags(NavigationState::SCHEDULED);
    }
  }
}

NavigationTracker::NavigationState::NavigationState() : state_bitfield_(0) {}

NavigationTracker::NavigationState::~NavigationState() {}

bool NavigationTracker::NavigationState::IsPendingNavigation() {
  return (state_bitfield_ & (LOADING | SCHEDULED)) != 0;
}

void NavigationTracker::NavigationState::SetFlags(const Mask mask) {
  state_bitfield_ |= mask;
}

void NavigationTracker::NavigationState::ClearFlags(const Mask mask) {
  state_bitfield_ &= ~mask;
}
