// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_

#include "components/gwp_asan/client/export.h"

namespace gwp_asan {

// Enable GWP-ASan for the current process. This should only be called once per
// process. This can not be disabled once it has been enabled. The caller should
// indicate whether this build is a canary or dev build.
GWP_ASAN_EXPORT void EnableForMalloc(bool is_canary_dev);

}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_
