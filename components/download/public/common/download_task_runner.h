// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_

#include "base/sequenced_task_runner.h"
#include "components/download/public/common/download_export.h"

namespace download {

// Returns the task runner used to save files and do other blocking operations.
COMPONENTS_DOWNLOAD_EXPORT scoped_refptr<base::SequencedTaskRunner>
GetDownloadTaskRunner();

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_
