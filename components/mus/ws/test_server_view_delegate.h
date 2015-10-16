// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_SERVER_VIEW_DELEGATE_H_
#define COMPONENTS_MUS_WS_TEST_SERVER_VIEW_DELEGATE_H_

#include "base/basictypes.h"
#include "components/mus/ws/server_view_delegate.h"

namespace mus {

struct ViewId;

class TestServerViewDelegate : public ServerViewDelegate {
 public:
  TestServerViewDelegate();
  ~TestServerViewDelegate() override;

  void set_root_view(const ServerView* view) { root_view_ = view; }

 private:
  // ServerViewDelegate:
  SurfacesState* GetSurfacesState() override;
  void OnScheduleViewPaint(const ServerView* view) override;
  const ServerView* GetRootView(const ServerView* view) const override;

  const ServerView* root_view_;

  DISALLOW_COPY_AND_ASSIGN(TestServerViewDelegate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_SERVER_VIEW_DELEGATE_H_
