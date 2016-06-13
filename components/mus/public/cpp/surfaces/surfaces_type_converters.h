// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_

#include <memory>

#include "cc/ipc/quads.mojom.h"
#include "cc/ipc/surface_id.mojom.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/cpp/surfaces/mojo_surfaces_export.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class CompositorFrame;
class CompositorFrameMetadata;
class DrawQuad;
class RenderPass;
class SharedQuadState;
}  // namespace cc

namespace mojo {

// Types from quads.mojom
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<cc::mojom::ColorPtr, SkColor> {
  static cc::mojom::ColorPtr Convert(const SkColor& input);
};
template <>
struct MOJO_SURFACES_EXPORT TypeConverter<SkColor, cc::mojom::ColorPtr> {
  static SkColor Convert(const cc::mojom::ColorPtr& input);
};

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<cc::mojom::DrawQuadPtr, cc::DrawQuad> {
  static cc::mojom::DrawQuadPtr Convert(const cc::DrawQuad& input);
};

std::unique_ptr<cc::RenderPass> ConvertToRenderPass(
    const cc::mojom::RenderPassPtr& input,
    const cc::CompositorFrameMetadata& metadata);

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<cc::mojom::RenderPassPtr, cc::RenderPass> {
  static cc::mojom::RenderPassPtr Convert(const cc::RenderPass& input);
};

template <>
struct MOJO_SURFACES_EXPORT
    TypeConverter<std::unique_ptr<cc::RenderPass>, cc::mojom::RenderPassPtr> {
  static std::unique_ptr<cc::RenderPass> Convert(
      const cc::mojom::RenderPassPtr& input);
};

// Types from compositor_frame.mojom
MOJO_SURFACES_EXPORT std::unique_ptr<cc::CompositorFrame>
ConvertToCompositorFrame(const mus::mojom::CompositorFramePtr& input);

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
