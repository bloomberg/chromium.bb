// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/test_server_view_delegate.h"
#include "components/view_manager/server_view.h"

namespace view_manager {

TestServerViewDelegate::TestServerViewDelegate() : root_view_(nullptr) {
}

TestServerViewDelegate::~TestServerViewDelegate() {
}

void TestServerViewDelegate::PrepareToDestroyView(ServerView* view) {
}

void TestServerViewDelegate::PrepareToChangeViewHierarchy(
    ServerView* view,
    ServerView* new_parent,
    ServerView* old_parent) {
}

void TestServerViewDelegate::PrepareToChangeViewVisibility(ServerView* view) {
}

void TestServerViewDelegate::OnScheduleViewPaint(const ServerView* view) {
}

const ServerView* TestServerViewDelegate::GetRootView(
    const ServerView* view) const {
  return root_view_;
}

}  // namespace view_manager
