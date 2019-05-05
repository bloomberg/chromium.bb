// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_

#include <memory>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace viz {

struct FuzzedBitmap {
  FuzzedBitmap(SharedBitmapId id,
               gfx::Size size,
               base::ReadOnlySharedMemoryRegion shared_region);
  ~FuzzedBitmap();

  FuzzedBitmap(FuzzedBitmap&& other) noexcept;
  FuzzedBitmap& operator=(FuzzedBitmap&& other) = default;

  SharedBitmapId id;
  gfx::Size size;
  base::ReadOnlySharedMemoryRegion shared_region;

  DISALLOW_COPY(FuzzedBitmap);
};

struct FuzzedData {
  FuzzedData();
  ~FuzzedData();

  FuzzedData(FuzzedData&& other) noexcept;
  FuzzedData& operator=(FuzzedData&& other) = default;

  CompositorFrame frame;
  std::vector<FuzzedBitmap> allocated_bitmaps;

  DISALLOW_COPY(FuzzedData);
};

class FuzzedCompositorFrameBuilder {
 public:
  FuzzedCompositorFrameBuilder();
  ~FuzzedCompositorFrameBuilder();

  // Builds a CompositorFrame with a root RenderPass matching the given protobuf
  // specification (see compositor_frame_fuzzer.proto), and allocates associated
  // resources.
  //
  // May elide quads in an attempt to impose a cap on memory allocated
  // to bitmaps over the frame's lifetime (from texture allocations to
  // intermediate bitmaps used to draw the final frame).
  //
  // If necessary, performs minimal correction to ensure submission to a
  // CompositorFrameSink will not cause validation errors on deserialization.
  FuzzedData Build(const content::fuzzing::proto::RenderPass& render_pass_spec);

 private:
  // Limit on number of bytes of memory to allocate (e.g. for referenced
  // bitmaps) in association with a single CompositorFrame. Currently 0.5 GiB;
  // reduce this if bots are running out of memory.
  static constexpr uint64_t kMaxTextureMemory = 1 << 29;

  RenderPassId AddRenderPass(
      const content::fuzzing::proto::RenderPass& render_pass_spec);

  // Helper methods for AddRenderPass. Try* methods may return before
  // creating the quad in order to adhere to memory limits.
  void AddSolidColorDrawQuad(
      RenderPass* pass,
      const gfx::Rect& rect,
      const gfx::Rect& visible_rect,
      const content::fuzzing::proto::DrawQuad& quad_spec);
  void TryAddTileDrawQuad(RenderPass* pass,
                          const gfx::Rect& rect,
                          const gfx::Rect& visible_rect,
                          const content::fuzzing::proto::DrawQuad& quad_spec);
  void TryAddRenderPassDrawQuad(
      RenderPass* pass,
      const gfx::Rect& rect,
      const gfx::Rect& visible_rect,
      const content::fuzzing::proto::DrawQuad& quad_spec);

  // Configure the SharedQuadState to match the specifications in the
  // protobuf, if they are defined for this quad. Otherwise, use sensible
  // defaults: match the |rect| and |visible_rect| of the corresponding quad,
  // and apply an identity transform.
  void ConfigureSharedQuadState(
      SharedQuadState* shared_quad_state,
      const content::fuzzing::proto::DrawQuad& quad_spec);

  // Records the intention to allocate enough memory for a bitmap of size
  // |size|, or returns false if the allocation would not be possible (the
  // size is 0 or the allocation would exceed kMaxMappedMemory).
  //
  // Should be called before AllocateFuzzedBitmap.
  bool TryReserveBitmapBytes(const gfx::Size& size);

  // Allocate and map memory for a bitmap filled with |color| and appends it
  // to |allocated_bitmaps|. Performs no checks to ensure that the bitmap will
  // fit in memory (see TryReserveBitmapBytes).
  FuzzedBitmap* AllocateFuzzedBitmap(const gfx::Size& size, SkColor color);

  // Number of bytes that have already been reserved for the allocation of
  // specific bitmaps/textures.
  uint64_t reserved_bytes_ = 0;

  RenderPassId next_pass_id_ = 1;

  // Frame and data being built.
  FuzzedData data_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedCompositorFrameBuilder);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
