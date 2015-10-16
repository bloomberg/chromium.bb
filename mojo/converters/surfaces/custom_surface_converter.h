// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_SURFACES_CUSTOM_SURFACE_CONVERTER_H_
#define MOJO_CONVERTERS_SURFACES_CUSTOM_SURFACE_CONVERTER_H_

namespace cc {
class RenderPass;
class SharedQuadState;
}  // namespace cc

namespace mojo {

// Classes that inherit from this converter can override the default behavior
// for converting a mojo::SurfaceDrawState to something cc understands.
class CustomSurfaceConverter {
 public:
  virtual bool ConvertSurfaceDrawQuad(
      const mus::mojom::QuadPtr& input,
      const mus::mojom::CompositorFrameMetadataPtr& metadata,
      cc::SharedQuadState* sqs,
      cc::RenderPass* render_pass) = 0;

 protected:
  virtual ~CustomSurfaceConverter() {}
};

} // namespace mojo

#endif  // MOJO_CONVERTERS_SURFACES_CUSTOM_SURFACE_CONVERTER_H_
