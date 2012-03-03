// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
#define CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
#pragma once

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "build/build_config.h"

namespace browser_sync {

namespace internal {
#if defined(OS_MACOSX)
// Returns the Hardware model name, without trailing numbers, if
// possible.  See http://www.cocoadev.com/index.pl?MacintoshModels for
// an example list of models. If an error occurs trying to read the
// model, this simply returns "Unknown".
std::string GetHardwareModelName();
#endif

#if defined(OS_WIN)
// Returns the computer name or the empty string if an error occured.
std::string GetComputerName();
#endif
}  // namespace internal

void GetSessionName(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const std::string&)>& done_callback);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
