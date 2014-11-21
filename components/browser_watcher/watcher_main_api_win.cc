// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_main_api_win.h"

namespace browser_watcher {

const base::FilePath::CharType kWatcherDll[] =
    FILE_PATH_LITERAL("chrome_watcher.dll");
const char kWatcherDLLEntrypoint[] = "WatcherMain";

}  // namespace browser_watcher
