// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_
#define CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_

#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

class CompositorFrameAck;

class CC_EXPORT OutputSurfaceClient {
 public:
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) = 0;
  virtual void DidVSync(base::TimeTicks frame_time) = 0;
  virtual void OnSendFrameToParentCompositorAck(const CompositorFrameAck&) = 0;

 protected:
  virtual ~OutputSurfaceClient() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_
