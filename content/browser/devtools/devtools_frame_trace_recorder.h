// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class DevToolsFrameTraceRecorderData;
class RenderFrameHostImpl;

class DevToolsFrameTraceRecorder {
 public:
  DevToolsFrameTraceRecorder();
  ~DevToolsFrameTraceRecorder();

  void OnSwapCompositorFrame(
      RenderFrameHostImpl* host,
      const cc::CompositorFrameMetadata& frame_metadata,
      bool do_capture);

 private:
  scoped_refptr<DevToolsFrameTraceRecorderData> pending_frame_data_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrameTraceRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_
