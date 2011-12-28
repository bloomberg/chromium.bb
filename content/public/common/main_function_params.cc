// Copyright 2011 Google Inc. All Rights Reserved.
// Author: ajwong@google.com (Albert Wong)
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/main_function_params.h"

namespace content {

MainFunctionParams::MainFunctionParams(const CommandLine& cl)
    : command_line(cl),
#if defined(OS_WIN)
      sandbox_info(NULL),
#elif defined(OS_MACOSX)
      autorelease_pool(NULL),
#endif
      ui_task() {
}

MainFunctionParams::~MainFunctionParams() {
}

}  // namespace content
