// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/returned_resource.h"

namespace cc {
class CompositorFrame;
}

namespace blimp {
namespace client {

// This class is a proxy for the BlimpOutputSurfaceClient to interact with the
// DelegatedOutputSurface. It should be called on the compositor thread only.
class BlimpOutputSurface {
 public:
  virtual ~BlimpOutputSurface() {}

  // Sent when the compositor can reclaim the |resources| references in a
  // compositor frame.
  virtual void ReclaimCompositorResources(
      const cc::ReturnedResourceArray& resources) = 0;
};

// This class is meant to be a proxy for the OutputSurface to interact with the
// consumer of the CompositorFrames. It should be called on the main thread
// only.
class BlimpOutputSurfaceClient {
 public:
  virtual ~BlimpOutputSurfaceClient() {}

  // Will be called before any frames are sent to the client. The weak ptr
  // provided here can be used to post messages to the output surface on the
  // compositor thread.
  virtual void BindToOutputSurface(
      base::WeakPtr<BlimpOutputSurface> output_surface) = 0;

  // The implementation should take the contents of the compositor frame and
  // return the referenced resources when the frame is no longer being drawn
  // using BlimpOutputSurface::ReclaimCompositorResources.
  virtual void SwapCompositorFrame(cc::CompositorFrame frame) = 0;

  // Will be called when the use of the OutputSurface is being terminated. No
  // calls from this output surface will be made to the client after this point.
  virtual void UnbindOutputSurface() = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
