// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_H_
#define CONTENT_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

struct MainFunctionParams;

namespace content {

bool ExitedMainMessageLoop();

}  // namespace content

CONTENT_EXPORT int BrowserMain(const MainFunctionParams& parameters);

#endif  // CONTENT_BROWSER_BROWSER_MAIN_H_
