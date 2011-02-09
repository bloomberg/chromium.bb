// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_plugin.h"

#include "chrome/common/automation_messages.h"

void ChromeFramePluginGetParamsCoordinates(const MiniContextMenuParams& params,
                                           int* x,
                                           int* y) {
  *x = params.screen_x;
  *y = params.screen_y;
}
