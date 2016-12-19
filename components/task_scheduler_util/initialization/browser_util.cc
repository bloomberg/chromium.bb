// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(fdoray): Remove this file once TaskScheduler initialization in the
// browser process uses the components/task_scheduler_util/browser/ API on all
// platforms.

#include "components/task_scheduler_util/initialization/browser_util.h"

#include "components/task_scheduler_util/browser/initialization.h"

namespace task_scheduler_util {
namespace initialization {

// Returns the worker pool index for |traits| defaulting to FOREGROUND or
// FOREGROUND_FILE_IO on any priorities other than background.
size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  return ::task_scheduler_util::BrowserWorkerPoolIndexForTraits(traits);
}

}  // namespace initialization
}  // namespace task_scheduler_util
