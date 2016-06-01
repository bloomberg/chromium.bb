// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_

#include <memory>

#include "cc/ipc/surface_id.mojom.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/cpp/surfaces/mojo_surfaces_export.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/quads.mojom.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class CompositorFrame;
class CompositorFrameMetadata;
class DrawQuad;
class RenderPass;
class RenderPassId;
class SharedQuadState;
}  // namespace cc

namespace mojo {

class CustomSurfaceConverter;

// Types from quads.mojom
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<mus::mojom::ColorPtr, SkColor> {
  static mus::mojom::ColorPtr Convert(const SkColor& input);
};
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<SkColor, mus::mojom::ColorPtr> {
  static SkColor Convert(const mus::mojom::ColorPtr& input);
};

template <>
struct MOJO_SURFACES_EXPORT TypeConverter<mus::mojom::QuadPtr, cc::DrawQuad> {
  static mus::mojom::QuadPtr Convert(const cc::DrawQuad& input);
};

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<mus::mojom::SharedQuadStatePtr, cc::SharedQuadState> {
  static mus::mojom::SharedQuadStatePtr Convert(
      const cc::SharedQuadState& input);
};

std::unique_ptr<cc::RenderPass> ConvertToRenderPass(
    const mus::mojom::PassPtr& input,
    const mus::mojom::CompositorFrameMetadataPtr& metadata,
    CustomSurfaceConverter* custom_converter);

template <>
struct MOJO_SURFACES_EXPORT TypeConverter<mus::mojom::PassPtr, cc::RenderPass> {
  static mus::mojom::PassPtr Convert(const cc::RenderPass& input);
};

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<std::unique_ptr<cc::RenderPass>, mus::mojom::PassPtr> {
  static std::unique_ptr<cc::RenderPass> Convert(
      const mus::mojom::PassPtr& input);
};

// Types from compositor_frame.mojom
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<mus::mojom::TransferableResourcePtr,
                                          cc::TransferableResource> {
  static mus::mojom::TransferableResourcePtr Convert(
      const cc::TransferableResource& input);
};
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<cc::TransferableResource,
                                          mus::mojom::TransferableResourcePtr> {
  static cc::TransferableResource Convert(
      const mus::mojom::TransferableResourcePtr& input);
};

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<mus::mojom::CompositorFrameMetadataPtr,
                  cc::CompositorFrameMetadata> {
  static mus::mojom::CompositorFrameMetadataPtr Convert(
      const cc::CompositorFrameMetadata& input);
};
template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<cc::CompositorFrameMetadata,
                  mus::mojom::CompositorFrameMetadataPtr> {
  static cc::CompositorFrameMetadata Convert(
      const mus::mojom::CompositorFrameMetadataPtr& input);
};

MOJO_SURFACES_EXPORT std::unique_ptr<cc::CompositorFrame>
ConvertToCompositorFrame(const mus::mojom::CompositorFramePtr& input,
                         CustomSurfaceConverter* custom_converter);

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<mus::mojom::CompositorFramePtr, cc::CompositorFrame> {
  static mus::mojom::CompositorFramePtr Convert(
      const cc::CompositorFrame& input);
};

template <>
struct MOJO_SURFACES_EXPORT TypeConverter<std::unique_ptr<cc::CompositorFrame>,
                                          mus::mojom::CompositorFramePtr> {
  static std::unique_ptr<cc::CompositorFrame> Convert(
      const mus::mojom::CompositorFramePtr& input);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
