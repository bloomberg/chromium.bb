// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RELEASE_CALLBACK_IMPL_H_
#define CC_RESOURCES_RELEASE_CALLBACK_IMPL_H_

#include "base/callback.h"

namespace cc {
class BlockingTaskRunner;

typedef base::Callback<void(uint32 sync_point,
                            bool is_lost,
                            BlockingTaskRunner* main_thread_task_runner)>
    ReleaseCallbackImpl;

}  // namespace cc

#endif  // CC_RESOURCES_RELEASE_CALLBACK_IMPL_H_
