// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cc/scheduler/begin_frame_source.h"

namespace content {
class SynchronousCompositorImpl;

// Make sure that this is initialized and set to compositor before output
// surface is bound to compositor.
class SynchronousCompositorExternalBeginFrameSource
    : public cc::BeginFrameSourceMixIn {
 public:
  explicit SynchronousCompositorExternalBeginFrameSource(int routing_id);
  ~SynchronousCompositorExternalBeginFrameSource() override;

  void BeginFrame();

  void SetCompositor(SynchronousCompositorImpl* compositor);

  // cc::BeginFrameSourceMixIn implementation.
  void OnNeedsBeginFramesChange(bool needs_begin_frames) override;
  void SetClientReady() override;

 private:
  bool CalledOnValidThread() const;

  const int routing_id_;
  bool registered_;

  // Not owned. This can be null when compositor is gone first than BFS.
  SynchronousCompositorImpl* compositor_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorExternalBeginFrameSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
