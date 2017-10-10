// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
#define CONTENT_ZYGOTE_ZYGOTE_MAIN_H_

#include <memory>
#include <vector>

#include "build/build_config.h"

namespace content {

struct MainFunctionParams;
class ZygoteForkDelegate;

bool ZygoteMain(
    const MainFunctionParams& params,
    std::vector<std::unique_ptr<ZygoteForkDelegate>> fork_delegates);

#if defined(OS_LINUX)
// On Linux, localtime is overridden to use a synchronous IPC to the browser
// process to determine the locale. This can be disabled, which causes
// localtime to use UTC instead. https://crbug.com/772503.
void DisableLocaltimeOverride();
#endif

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
