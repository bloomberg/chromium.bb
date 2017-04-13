// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/toolbar/toolbar_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ToolbarTestWebState::ToolbarTestWebState() : loading_progress_(0){};

double ToolbarTestWebState::GetLoadingProgress() const {
  return loading_progress_;
}

void ToolbarTestWebState::set_loading_progress(double loading_progress) {
  loading_progress_ = loading_progress;
}

ToolbarTestNavigationManager::ToolbarTestNavigationManager()
    : can_go_back_(false), can_go_forward_(false) {}

bool ToolbarTestNavigationManager::CanGoBack() const {
  return can_go_back_;
}

bool ToolbarTestNavigationManager::CanGoForward() const {
  return can_go_forward_;
}

void ToolbarTestNavigationManager::set_can_go_back(bool can_go_back) {
  can_go_back_ = can_go_back;
}

void ToolbarTestNavigationManager::set_can_go_forward(bool can_go_forward) {
  can_go_forward_ = can_go_forward;
}
