// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_FRAME_DATA_H_
#define CC_OUTPUT_SOFTWARE_FRAME_DATA_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

namespace cc {

class CC_EXPORT SoftwareFrameData {
 public:
  SoftwareFrameData();
  ~SoftwareFrameData();

  gfx::Size size;
  gfx::Rect damage_rect;
  TransportDIB::Id dib_id;
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_FRAME_DATA_H_
