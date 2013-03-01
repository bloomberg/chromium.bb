// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GL_FRAME_DATA_H_
#define CC_GL_FRAME_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/transferable_resource.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT GLFrameData {
 public:
  GLFrameData();
  ~GLFrameData();

  Mailbox mailbox;
  uint32 sync_point;
  gfx::Size size;
};

}  // namespace cc

#endif  // CC_GL_FRAME_DATA_H_
