// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CC_SURFACES_FRAMESINK_MANAGER_CLIENT_H_
#define CC_SURFACES_FRAMESINK_MANAGER_CLIENT_H_

#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class CC_SURFACES_EXPORT FrameSinkManagerClient {
 public:
  virtual ~FrameSinkManagerClient() = default;

  // This allows the FrameSinkManager to pass a BeginFrameSource to use.
  virtual void SetBeginFrameSource(BeginFrameSource* begin_frame_source) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_FRAMESINK_MANAGER_CLIENT_H_
