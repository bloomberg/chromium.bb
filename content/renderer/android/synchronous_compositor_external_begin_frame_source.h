// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/scheduler/begin_frame_source.h"

namespace content {

class SynchronousCompositorRegistry;

class SynchronousCompositorExternalBeginFrameSourceClient {
 public:
  virtual void OnNeedsBeginFramesChange(bool needs_begin_frames) = 0;

 protected:
  virtual ~SynchronousCompositorExternalBeginFrameSourceClient() {}
};

// Make sure that this is initialized and set to client before output
// surface is bound to compositor.
class SynchronousCompositorExternalBeginFrameSource
    : public cc::BeginFrameSourceBase {
 public:
  SynchronousCompositorExternalBeginFrameSource(
      int routing_id,
      SynchronousCompositorRegistry* registry);
  ~SynchronousCompositorExternalBeginFrameSource() override;

  void BeginFrame(const cc::BeginFrameArgs& args);
  void SetClient(SynchronousCompositorExternalBeginFrameSourceClient* client);
  using cc::BeginFrameSourceBase::SetBeginFrameSourcePaused;

  // cc::BeginFrameSourceBase implementation.
  void OnNeedsBeginFramesChanged(bool needs_begin_frames) override;
  void SetClientReady() override;

 private:
  bool CalledOnValidThread() const;

  const int routing_id_;
  SynchronousCompositorRegistry* const registry_;
  bool registered_;

  // Not owned.
  SynchronousCompositorExternalBeginFrameSourceClient* client_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorExternalBeginFrameSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
