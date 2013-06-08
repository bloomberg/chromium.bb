// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_FRAME_DATA_H_
#define CC_OUTPUT_SOFTWARE_FRAME_DATA_H_

#include "base/memory/shared_memory.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/rect.h"

namespace cc {

class CC_EXPORT SoftwareFrameData {
 public:
  SoftwareFrameData();
  ~SoftwareFrameData();

  unsigned id;
  gfx::Size size;
  gfx::Rect damage_rect;
  base::SharedMemoryHandle handle;
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_FRAME_DATA_H_
