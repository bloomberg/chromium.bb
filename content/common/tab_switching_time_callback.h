// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TAB_SWITCHING_TIME_CALLBACK_H_
#define CONTENT_COMMON_TAB_SWITCHING_TIME_CALLBACK_H_

#include "base/callback_forward.h"
#include "base/time/time.h"

namespace content {

base::OnceCallback<void(base::TimeTicks, base::TimeDelta, uint32_t)>
CreateTabSwitchingTimeRecorder(base::TimeTicks request_time);

}  // namespace content

#endif  // CONTENT_COMMON_TAB_SWITCHING_TIME_CALLBACK_H_
