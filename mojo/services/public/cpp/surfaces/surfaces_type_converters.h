// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "mojo/services/public/cpp/surfaces/mojo_surfaces_export.h"
#include "mojo/services/public/interfaces/surfaces/quads.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class CompositorFrame;
class DrawQuad;
class RenderPass;
class SharedQuadState;
}  // namespace cc

namespace mojo {

// Types from surface_id.mojom
template <>
class MOJO_SURFACES_EXPORT
TypeConverter<SurfaceIdPtr, cc::SurfaceId> {
 public:
  static SurfaceIdPtr ConvertFrom(const cc::SurfaceId& input);
  static cc::SurfaceId ConvertTo(const SurfaceIdPtr& input);
};

// Types from quads.mojom
template <>
class MOJO_SURFACES_EXPORT TypeConverter<ColorPtr, SkColor> {
 public:
  static ColorPtr ConvertFrom(const SkColor& input);
  static SkColor ConvertTo(const ColorPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT TypeConverter<QuadPtr, cc::DrawQuad> {
 public:
  static QuadPtr ConvertFrom(const cc::DrawQuad& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<SharedQuadStatePtr, cc::SharedQuadState> {
 public:
  static SharedQuadStatePtr ConvertFrom(const cc::SharedQuadState& input);
};

template <>
class MOJO_SURFACES_EXPORT TypeConverter<PassPtr, cc::RenderPass> {
 public:
  static PassPtr ConvertFrom(const cc::RenderPass& input);
};

// This can't use the TypeConverter since cc::RenderPass must be heap allocated
// and isn't copyable.
MOJO_SURFACES_EXPORT scoped_ptr<cc::RenderPass> ConvertTo(
    const PassPtr& input);

// Types from surfaces.mojom
template <>
class MOJO_SURFACES_EXPORT TypeConverter<MailboxPtr, gpu::Mailbox> {
 public:
  static MailboxPtr ConvertFrom(const gpu::Mailbox& input);
  static gpu::Mailbox ConvertTo(const MailboxPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<MailboxHolderPtr, gpu::MailboxHolder> {
 public:
  static MailboxHolderPtr ConvertFrom(const gpu::MailboxHolder& input);
  static gpu::MailboxHolder ConvertTo(const MailboxHolderPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<TransferableResourcePtr, cc::TransferableResource> {
 public:
  static TransferableResourcePtr ConvertFrom(
      const cc::TransferableResource& input);
  static cc::TransferableResource ConvertTo(
      const TransferableResourcePtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<Array<TransferableResourcePtr>, cc::TransferableResourceArray> {
 public:
  static Array<TransferableResourcePtr> ConvertFrom(
      const cc::TransferableResourceArray& input);
  static cc::TransferableResourceArray ConvertTo(
      const Array<TransferableResourcePtr>& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<ReturnedResourcePtr, cc::ReturnedResource> {
 public:
  static ReturnedResourcePtr ConvertFrom(const cc::ReturnedResource& input);
  static cc::ReturnedResource ConvertTo(const ReturnedResourcePtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<Array<ReturnedResourcePtr>, cc::ReturnedResourceArray> {
 public:
  static Array<ReturnedResourcePtr> ConvertFrom(
      const cc::ReturnedResourceArray& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<FramePtr, cc::CompositorFrame> {
 public:
  static FramePtr ConvertFrom(const cc::CompositorFrame& input);
};

MOJO_SURFACES_EXPORT scoped_ptr<cc::CompositorFrame> ConvertTo(
    const FramePtr& input);

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
