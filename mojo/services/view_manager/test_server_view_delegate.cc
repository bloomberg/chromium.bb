// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/test_server_view_delegate.h"

namespace view_manager {

TestServerViewDelegate::TestServerViewDelegate() {
}

TestServerViewDelegate::~TestServerViewDelegate() {
}

void TestServerViewDelegate::OnWillDestroyView(ServerView* view) {
}

void TestServerViewDelegate::OnViewDestroyed(const ServerView* view) {
}

void TestServerViewDelegate::OnWillChangeViewHierarchy(ServerView* view,
                                                       ServerView* new_parent,
                                                       ServerView* old_parent) {
}

void TestServerViewDelegate::OnViewHierarchyChanged(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
}

void TestServerViewDelegate::OnViewBoundsChanged(const ServerView* view,
                                                 const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds) {
}

void TestServerViewDelegate::OnViewSurfaceIdChanged(const ServerView* view) {
}

void TestServerViewDelegate::OnViewReordered(const ServerView* view,
                                             const ServerView* relative,
                                             mojo::OrderDirection direction) {
}

void TestServerViewDelegate::OnWillChangeViewVisibility(ServerView* view) {
}

void TestServerViewDelegate::OnViewSharedPropertyChanged(
    const ServerView* view,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
}

void TestServerViewDelegate::OnScheduleViewPaint(const ServerView* view) {
}

}  // namespace view_manager
