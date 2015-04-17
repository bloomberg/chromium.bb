// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/test_server_view_delegate.h"

namespace view_manager {

TestServerViewDelegate::TestServerViewDelegate() {
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

}  // namespace view_manager
