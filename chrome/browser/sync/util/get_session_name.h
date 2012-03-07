// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
#define CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
#pragma once

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace browser_sync {

void GetSessionName(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const std::string&)>& done_callback);

std::string GetSessionNameSynchronouslyForTesting();

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_H_
