// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_ANDROID_SYNCHRONOUS_COMPOSTIOR_
#define CONTENT_PUBLIC_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_

class SkCanvas;

namespace content {

class SynchronousCompositorClient;

// Interface for embedders that which to direct compositing operations
// synchronously under their own control. Only meaningful when the
// kEnableSyncrhonousRendererCompositor flag is specified.
class SynchronousCompositor {
 public:
  // Allows changing or resetting the client to NULL (this must be used if
  // the client is being deleted prior to the DidDestroyCompositor() call
  // being received by the client). Ownership of |client| remains with
  // the caller.
  virtual void SetClient(SynchronousCompositorClient* client) = 0;

  // "On demand" SW draw, into the supplied canvas (observing the transform
  // and clip set there-in).
  virtual bool DemandDrawSw(SkCanvas* canvas) = 0;

 protected:
  virtual ~SynchronousCompositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_ANDROID_SYNCHRONOUS_COMPOSTIOR_
