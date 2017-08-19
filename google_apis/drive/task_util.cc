// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/task_util.h"

#include "base/location.h"

namespace google_apis {

void RunTaskWithTaskRunner(scoped_refptr<base::TaskRunner> task_runner,
                           base::OnceClosure task) {
  const bool posted = task_runner->PostTask(FROM_HERE, std::move(task));
  DCHECK(posted);
}

}  // namespace google_apis
