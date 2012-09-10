// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller.h"

#include "build/build_config.h"

namespace app_list_controller {

#if !defined(OS_WIN)
// Default implementation for ports which do not have this implemented.
void ShowAppList() {}
#endif

}  // namespace app_list_controller
