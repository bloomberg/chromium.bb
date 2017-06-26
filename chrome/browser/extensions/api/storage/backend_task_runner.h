// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_BACKEND_TASK_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_BACKEND_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

namespace extensions {

// Gets the singleton task runner for running SyncStorageBackend on.
scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_BACKEND_TASK_RUNNER_H_
