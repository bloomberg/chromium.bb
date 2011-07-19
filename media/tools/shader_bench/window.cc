// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/window.h"

#include "base/task.h"

namespace media {

Window::Window(int width, int height)
    : done_task_(NULL),
      painter_(NULL),
      limit_(0),
      count_(0),
      running_(false) {
  window_handle_ = CreateNativeWindow(width, height);
}

}  // namespace media
