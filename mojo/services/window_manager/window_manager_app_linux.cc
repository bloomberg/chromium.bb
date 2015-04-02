// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

namespace window_manager {

ui::Accelerator WindowManagerApp::ConvertEventToAccelerator(
    const ui::KeyEvent* event) {
  return ui::Accelerator(event->key_code(), event->flags());
}

}  // namespace window_manager
