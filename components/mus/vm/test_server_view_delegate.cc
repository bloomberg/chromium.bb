// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/vm/test_server_view_delegate.h"
#include "components/mus/vm/server_view.h"

namespace mus {

TestServerViewDelegate::TestServerViewDelegate() : root_view_(nullptr) {}

TestServerViewDelegate::~TestServerViewDelegate() {}

SurfacesState* TestServerViewDelegate::GetSurfacesState() {
  return nullptr;
}

void TestServerViewDelegate::OnScheduleViewPaint(const ServerView* view) {}

const ServerView* TestServerViewDelegate::GetRootView(
    const ServerView* view) const {
  return root_view_;
}

}  // namespace mus
