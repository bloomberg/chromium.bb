// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TASK_RUNNER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TASK_RUNNER_H_

#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"

namespace content {

// Returns the task runner used to save files and do other blocking operations.
CONTENT_EXPORT scoped_refptr<base::SequencedTaskRunner> GetDownloadTaskRunner();

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TASK_RUNNER_H_
