// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_LIBSECRET_TASK_RUNNER_LINUX_H_
#define COMPONENTS_OS_CRYPT_LIBSECRET_TASK_RUNNER_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

// Concurrent calls to the libsecret library may sometimes cause erroneous
// behaviour. Use this TaskRunner to remove race conditions between all
// components interacting with libsecret.

namespace os_crypt {

scoped_refptr<base::SequencedTaskRunner> GetLibsecretTaskRunner();

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_LIBSECRET_TASK_RUNNER_LINUX_H_
