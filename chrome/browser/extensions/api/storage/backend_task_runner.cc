// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/backend_task_runner.h"

#include "content/public/browser/browser_thread.h"

namespace extensions {

// TODO(stanisc): crbug.com/731903: change to LazySequencedTaskRunner once
// migration from BrowserThread::FILE to GetBackendTaskRunner() is complete.
scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() {
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::FILE);
}

}  // namespace extensions
