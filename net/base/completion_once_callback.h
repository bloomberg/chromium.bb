// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_ONCE_CALLBACK_H_
#define NET_BASE_COMPLETION_ONCE_CALLBACK_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/cancelable_callback.h"

namespace net {

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
typedef base::OnceCallback<void(int)> CompletionOnceCallback;

// 64bit version of the OnceCallback specialization that takes a single int64_t
// parameter. Usually this is used to report a file offset, size or network
// error code.
typedef base::OnceCallback<void(int64_t)> Int64CompletionOnceCallback;

typedef base::CancelableOnceCallback<void(int)>
    CancelableCompletionOnceCallback;

}  // namespace net

#endif  // NET_BASE_COMPLETION_ONCE_CALLBACK_H_
