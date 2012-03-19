// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_filter_test_helper.h"

namespace ash {
namespace internal {

WorkspaceEventFilterTestHelper::WorkspaceEventFilterTestHelper(
    WorkspaceEventFilter* filter)
    : filter_(filter) {
}

WorkspaceEventFilterTestHelper::~WorkspaceEventFilterTestHelper() {
}

}  // namespace internal
}  // namespace ash
