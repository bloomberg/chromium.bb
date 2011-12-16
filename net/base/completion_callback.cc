// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/completion_callback.h"

namespace net {

void OldCompletionCallbackAdapter(OldCompletionCallback* callback, int result) {
  callback->Run(result);
}

}  // namespace net
