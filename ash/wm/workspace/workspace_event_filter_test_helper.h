// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_TEST_HELPER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_TEST_HELPER_H_
#pragma once

#include "ash/wm/workspace/workspace_event_filter.h"

namespace ash {
namespace internal {

class WorkspaceEventFilterTestHelper {
 public:
  explicit WorkspaceEventFilterTestHelper(WorkspaceEventFilter* filter);
  ~WorkspaceEventFilterTestHelper();

  MultiWindowResizeController* resize_controller() {
    return &(filter_->multi_window_resize_controller_);
  }

 private:
  WorkspaceEventFilter* filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventFilterTestHelper);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_TEST_HELPER_H_
