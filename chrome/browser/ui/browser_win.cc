// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"

void Browser::SetMetroSnapMode(bool enable) {
  fullscreen_controller_->SetMetroSnapMode(enable);
}
