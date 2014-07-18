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
TypeConverter<surfaces::SurfaceIdPtr, cc::SurfaceId> {
 public:
  static surfaces::SurfaceIdPtr ConvertFrom(const cc::SurfaceId& input);
  static cc::SurfaceId ConvertTo(const surfaces::SurfaceIdPtr& input);
};

// Types from quads.mojom
template <>
class MOJO_SURFACES_EXPORT TypeConverter<surfaces::ColorPtr, SkColor> {
 public:
  static surfaces::ColorPtr ConvertFrom(const SkColor& input);
  static SkColor ConvertTo(const surfaces::ColorPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT TypeConverter<surfaces::QuadPtr, cc::DrawQuad> {
 public:
  static surfaces::QuadPtr ConvertFrom(const cc::DrawQuad& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<surfaces::SharedQuadStatePtr, cc::SharedQuadState> {
 public:
  static surfaces::SharedQuadStatePtr ConvertFrom(
      const cc::SharedQuadState& input);
};

template <>
class MOJO_SURFACES_EXPORT TypeConverter<surfaces::PassPtr, cc::RenderPass> {
 public:
  static surfaces::PassPtr ConvertFrom(const cc::RenderPass& input);
};

// This can't use the TypeConverter since cc::RenderPass must be heap allocated
// and isn't copyable.
MOJO_SURFACES_EXPORT scoped_ptr<cc::RenderPass> ConvertTo(
    const surfaces::PassPtr& input);

// Types from surfaces.mojom
template <>
class MOJO_SURFACES_EXPORT TypeConverter<surfaces::MailboxPtr, gpu::Mailbox> {
 public:
  static surfaces::MailboxPtr ConvertFrom(const gpu::Mailbox& input);
  static gpu::Mailbox ConvertTo(const surfaces::MailboxPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<surfaces::MailboxHolderPtr, gpu::MailboxHolder> {
 public:
  static surfaces::MailboxHolderPtr ConvertFrom(
      const gpu::MailboxHolder& input);
  static gpu::MailboxHolder ConvertTo(const surfaces::MailboxHolderPtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<surfaces::TransferableResourcePtr, cc::TransferableResource> {
 public:
  static surfaces::TransferableResourcePtr ConvertFrom(
      const cc::TransferableResource& input);
  static cc::TransferableResource ConvertTo(
      const surfaces::TransferableResourcePtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<Array<surfaces::TransferableResourcePtr>,
              cc::TransferableResourceArray> {
 public:
  static Array<surfaces::TransferableResourcePtr> ConvertFrom(
      const cc::TransferableResourceArray& input);
  static cc::TransferableResourceArray ConvertTo(
      const Array<surfaces::TransferableResourcePtr>& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<surfaces::ReturnedResourcePtr, cc::ReturnedResource> {
 public:
  static surfaces::ReturnedResourcePtr ConvertFrom(
      const cc::ReturnedResource& input);
  static cc::ReturnedResource ConvertTo(
      const surfaces::ReturnedResourcePtr& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<Array<surfaces::ReturnedResourcePtr>, cc::ReturnedResourceArray> {
 public:
  static Array<surfaces::ReturnedResourcePtr> ConvertFrom(
      const cc::ReturnedResourceArray& input);
};

template <>
class MOJO_SURFACES_EXPORT
TypeConverter<surfaces::FramePtr, cc::CompositorFrame> {
 public:
  static surfaces::FramePtr ConvertFrom(const cc::CompositorFrame& input);
};

MOJO_SURFACES_EXPORT scoped_ptr<cc::CompositorFrame> ConvertTo(
    const surfaces::FramePtr& input);

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_SURFACES_SURFACES_TYPE_CONVERTERS_H_
