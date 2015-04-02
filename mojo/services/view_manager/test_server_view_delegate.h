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
  void OnWillDestroyView(ServerView* view) override;
  void OnViewDestroyed(const ServerView* view) override;
  void OnWillChangeViewHierarchy(ServerView* view,
                                 ServerView* new_parent,
                                 ServerView* old_parent) override;
  void OnViewHierarchyChanged(const ServerView* view,
                              const ServerView* new_parent,
                              const ServerView* old_parent) override;
  void OnViewBoundsChanged(const ServerView* view,
                           const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) override;
  void OnViewSurfaceIdChanged(const ServerView* view) override;
  void OnViewReordered(const ServerView* view,
                       const ServerView* relative,
                       mojo::OrderDirection direction) override;
  void OnWillChangeViewVisibility(ServerView* view) override;
  void OnViewSharedPropertyChanged(
      const ServerView* view,
      const std::string& name,
      const std::vector<uint8_t>* new_data) override;
  void OnScheduleViewPaint(const ServerView* view) override;

  DISALLOW_COPY_AND_ASSIGN(TestServerViewDelegate);
};

}  // namespace view_manager

#endif  // SERVICES_VIEW_MANAGER_TEST_SERVER_VIEW_DELEGATE_H_
