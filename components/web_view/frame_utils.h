// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_UTILS_H_
#define COMPONENTS_WEB_VIEW_FRAME_UTILS_H_

#include <stdint.h>

namespace web_view {

// Returns true if the app ids should be considered the same.
bool AreAppIdsEqual(uint32_t app_id1, uint32_t app_id2);

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_UTILS_H_
