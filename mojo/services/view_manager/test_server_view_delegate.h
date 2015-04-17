// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIEW_MANAGER_TEST_SERVER_VIEW_DELEGATE_H_
#define SERVICES_VIEW_MANAGER_TEST_SERVER_VIEW_DELEGATE_H_

#include "base/basictypes.h"
#include "mojo/services/view_manager/server_view_delegate.h"

namespace view_manager {

class TestServerViewDelegate : public ServerViewDelegate {
 public:
  TestServerViewDelegate();
  ~TestServerViewDelegate() override;

 private:
  // ServerViewDelegate:
  void PrepareToDestroyView(ServerView* view) override;
  void PrepareToChangeViewHierarchy(ServerView* view,
                                    ServerView* new_parent,
                                    ServerView* old_parent) override;
  void PrepareToChangeViewVisibility(ServerView* view) override;
  void OnScheduleViewPaint(const ServerView* view) override;

  DISALLOW_COPY_AND_ASSIGN(TestServerViewDelegate);
};

}  // namespace view_manager

#endif  // SERVICES_VIEW_MANAGER_TEST_SERVER_VIEW_DELEGATE_H_
