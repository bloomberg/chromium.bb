// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_


namespace content {

class SynchronousCompositorExternalBeginFrameSource;
class SynchronousCompositorOutputSurface;

class SynchronousCompositorRegistry {
 public:
  virtual void RegisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) = 0;
  virtual void UnregisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) = 0;
  virtual void RegisterBeginFrameSource(
      int routing_id,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source) = 0;
  virtual void UnregisterBeginFrameSource(
      int routing_id,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source) = 0;

 protected:
  virtual ~SynchronousCompositorRegistry() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
