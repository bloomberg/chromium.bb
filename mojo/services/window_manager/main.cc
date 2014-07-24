// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/window_manager/window_manager_app.h"

// ApplicationDelegate implementation file for WindowManager users (e.g.
// core window manager tests) that do not want to provide their own
// ApplicationDelegate::Create().

namespace mojo {

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new WindowManagerApp(NULL);
}

}  // namespace mojo
