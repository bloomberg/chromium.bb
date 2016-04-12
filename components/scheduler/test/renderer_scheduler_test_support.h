// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

namespace scheduler {

class RendererScheduler;

extern void RunIdleTasksForTesting(RendererScheduler* scheduler,
                                   const base::Closure& callback);

}
