// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer_serializer.h"

#include "base/bind.h"
#include "cc/paint/scoped_raster_flags.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

class ScopedFlagsOverride {
 public:
  ScopedFlagsOverride(PaintOp::SerializeOptions* options,
                      const PaintFlags* flags)
      : options_(options) {
    options_->flags_to_serialize = flags;
  }
  ~ScopedFlagsOverride() { options_->flags_to_serialize = nullptr; }

 private:
  PaintOp::SerializeOptions* options_;
};

}  // namespace

PaintOpBufferSerializer::PaintOpBufferSerializer(
    SerializeCallback serialize_cb,
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache)
    : serialize_cb_(std::move(serialize_cb)),
      canvas_(100, 100),
      image_provider_(image_provider),
      transfer_cache_(transfer_cache) {
  DCHECK(serialize_cb_);
}

PaintOpBufferSerializer::~PaintOpBufferSerializer() = default;

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer,
                                        const std::vector<size_t>* offsets,
                                        const Preamble& preamble) {
  canvas_.resetCanvas(preamble.full_raster_rect.width(),
                      preamble.full_raster_rect.height());
  DCHECK(canvas_.getTotalMatrix().isIdentity());
  static const int kInitialSaveCount = 1;
  DCHECK_EQ(kInitialSaveCount, canvas_.getSaveCount());

  // These SerializeOptions and PlaybackParams use the initial (identity) canvas
  // matrix, as they are only used for serializing the preamble and the initial
  // save / final restore. SerializeBuffer will create its own SerializeOptions
  // and PlaybackParams based on the post-preamble canvas.
  PaintOp::SerializeOptions options(image_provider_, transfer_cache_, &canvas_,
                                    canvas_.getTotalMatrix());
  PlaybackParams params(image_provider_, canvas_.getTotalMatrix());

  Save(options, params);
  SerializePreamble(preamble, options, params);
  SerializeBuffer(buffer, offsets);
  RestoreToCount(kInitialSaveCount, options, params);
}

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer) {
  // Use half of the max int as the extent for the SkNoDrawCanvas.
  static const int extent = std::numeric_limits<int>::max() >> 1;
  // Reset the canvas to the maximum extents of our playback rect, ensuring this
  // rect will not reject images.
  canvas_.resetCanvas(extent, extent);
  DCHECK(canvas_.getTotalMatrix().isIdentity());

  SerializeBuffer(buffer, nullptr);
}

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer,
                                        const gfx::Rect& playback_rect,
                                        const gfx::SizeF& post_scale) {
  // Reset the canvas to the maximum extents of our playback rect, ensuring this
  // rect will not reject images.
  canvas_.resetCanvas(playback_rect.width(), playback_rect.height());
  DCHECK(canvas_.getTotalMatrix().isIdentity());

  PaintOp::SerializeOptions options(image_provider_, transfer_cache_, &canvas_,
                                    canvas_.getTotalMatrix());
  PlaybackParams params(image_provider_, canvas_.getTotalMatrix());

  // TODO(khushalsagar): remove this clip rect if it's not needed.
  if (!playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(playback_rect), SkClipOp::kIntersect,
                       false);
    SerializeOp(&clip_op, options, params);
  }

  if (post_scale.width() != 1.f || post_scale.height() != 1.f) {
    ScaleOp scale_op(post_scale.width(), post_scale.height());
    SerializeOp(&scale_op, options, params);
  }

  SerializeBuffer(buffer, nullptr);
}

void PaintOpBufferSerializer::SerializePreamble(
    const Preamble& preamble,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  DCHECK(preamble.full_raster_rect.Contains(preamble.playback_rect))
      << "full: " << preamble.full_raster_rect.ToString()
      << ", playback: " << preamble.playback_rect.ToString();

  // Should full clears be clipped?
  bool is_partial_raster = preamble.full_raster_rect != preamble.playback_rect;

  // If rastering the entire tile, clear pre-clip.  This is so that any
  // external texels outside of the playback rect also get cleared.  There's
  // not enough information at this point to know if this texture is being
  // reused from another tile, so the external texels could have been
  // cleared to some wrong value.
  if (preamble.requires_clear && !is_partial_raster) {
    // If the tile is transparent, then just clear the whole thing.
    DrawColorOp clear(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    SerializeOp(&clear, options, params);
  } else if (!is_partial_raster) {
    // The last texel of this content is not guaranteed to be fully opaque, so
    // inset by one to generate the fully opaque coverage rect .  This rect is
    // in device space.
    SkIRect coverage_device_rect = SkIRect::MakeWH(
        preamble.content_size.width() - preamble.full_raster_rect.x() - 1,
        preamble.content_size.height() - preamble.full_raster_rect.y() - 1);

    SkIRect playback_device_rect = gfx::RectToSkIRect(preamble.playback_rect);
    playback_device_rect.fLeft -= preamble.full_raster_rect.x();
    playback_device_rect.fTop -= preamble.full_raster_rect.y();

    // If not fully covered, we need to clear one texel inside the coverage rect
    // (because of blending during raster) and one texel outside the full raster
    // rect (because of bilinear filtering during draw).  See comments in
    // RasterSource.
    SkIRect device_column = SkIRect::MakeXYWH(coverage_device_rect.right(), 0,
                                              2, coverage_device_rect.bottom());
    // row includes the corner, column excludes it.
    SkIRect device_row = SkIRect::MakeXYWH(0, coverage_device_rect.bottom(),
                                           coverage_device_rect.right() + 2, 2);
    // Only bother clearing if we need to.
    if (SkIRect::Intersects(device_column, playback_device_rect)) {
      Save(options, params);
      ClipRectOp clip_op(SkRect::MakeFromIRect(device_column),
                         SkClipOp::kIntersect, false);
      SerializeOp(&clip_op, options, params);
      DrawColorOp clear_op(preamble.background_color, SkBlendMode::kSrc);
      SerializeOp(&clear_op, options, params);
      RestoreToCount(1, options, params);
    }
    if (SkIRect::Intersects(device_row, playback_device_rect)) {
      Save(options, params);
      ClipRectOp clip_op(SkRect::MakeFromIRect(device_row),
                         SkClipOp::kIntersect, false);
      SerializeOp(&clip_op, options, params);
      DrawColorOp clear_op(preamble.background_color, SkBlendMode::kSrc);
      SerializeOp(&clear_op, options, params);
      RestoreToCount(1, options, params);
    }
  }

  if (!preamble.full_raster_rect.OffsetFromOrigin().IsZero()) {
    TranslateOp translate_op(-preamble.full_raster_rect.x(),
                             -preamble.full_raster_rect.y());
    SerializeOp(&translate_op, options, params);
  }

  if (!preamble.playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(preamble.playback_rect),
                       SkClipOp::kIntersect, false);
    SerializeOp(&clip_op, options, params);
  }

  if (!preamble.post_translation.IsZero()) {
    TranslateOp translate_op(preamble.post_translation.x(),
                             preamble.post_translation.y());
    SerializeOp(&translate_op, options, params);
  }

  if (preamble.post_scale.width() != 1.f ||
      preamble.post_scale.height() != 1.f) {
    ScaleOp scale_op(preamble.post_scale.width(), preamble.post_scale.height());
    SerializeOp(&scale_op, options, params);
  }

  // If tile is transparent and this is partial raster, just clear the
  // section that is being rastered.  If this is opaque, trust the raster
  // to write all the pixels inside of the full_raster_rect.
  if (preamble.requires_clear && is_partial_raster) {
    DrawColorOp clear_op(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    SerializeOp(&clear_op, options, params);
  }
}

void PaintOpBufferSerializer::SerializeBuffer(
    const PaintOpBuffer* buffer,
    const std::vector<size_t>* offsets) {
  DCHECK(buffer);
  PaintOp::SerializeOptions options(image_provider_, transfer_cache_, &canvas_,
                                    canvas_.getTotalMatrix());
  PlaybackParams params(image_provider_, canvas_.getTotalMatrix());

  for (PaintOpBuffer::PlaybackFoldingIterator iter(buffer, offsets); iter;
       ++iter) {
    const PaintOp* op = *iter;

    // Skip ops outside the current clip if they have images. This saves
    // performing an unnecessary expensive decode.
    const bool skip_op = PaintOp::OpHasDiscardableImages(op) &&
                         PaintOp::QuickRejectDraw(op, &canvas_);
    if (skip_op)
      continue;

    if (op->GetType() != PaintOpType::DrawRecord) {
      bool success = false;
      if (op->IsPaintOpWithFlags()) {
        success = SerializeOpWithFlags(static_cast<const PaintOpWithFlags*>(op),
                                       &options, params, iter.alpha());
      } else {
        success = SerializeOp(op, options, params);
      }

      if (!success)
        return;
      continue;
    }

    int save_count = canvas_.getSaveCount();
    Save(options, params);
    SerializeBuffer(static_cast<const DrawRecordOp*>(op)->record.get(),
                    nullptr);
    RestoreToCount(save_count, options, params);
  }
}

bool PaintOpBufferSerializer::SerializeOpWithFlags(
    const PaintOpWithFlags* flags_op,
    PaintOp::SerializeOptions* options,
    const PlaybackParams& params,
    uint8_t alpha) {
  // We don't need the skia backing for decoded shaders during serialization,
  // since those are created on the service side where the record is rasterized.
  const bool create_skia_shaders = false;

  const ScopedRasterFlags scoped_flags(
      &flags_op->flags, options->image_provider,
      options->canvas->getTotalMatrix(), alpha, create_skia_shaders);
  const PaintFlags* flags_to_serialize = scoped_flags.flags();
  if (!flags_to_serialize)
    return true;

  ScopedFlagsOverride override_flags(options, flags_to_serialize);
  return SerializeOp(flags_op, *options, params);
}

bool PaintOpBufferSerializer::SerializeOp(
    const PaintOp* op,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  if (!valid_)
    return false;

  size_t bytes = serialize_cb_.Run(op, options);
  if (!bytes) {
    valid_ = false;
    return false;
  }

  DCHECK_GE(bytes, 4u);
  DCHECK_EQ(bytes % PaintOpBuffer::PaintOpAlign, 0u);

  // Only pass state-changing operations to the canvas.
  if (!op->IsDrawOp()) {
    // Note that we don't need to use overridden flags during raster here since
    // the override must not affect any state being tracked by this canvas.
    op->Raster(&canvas_, params);
  }
  return true;
}

void PaintOpBufferSerializer::Save(const PaintOp::SerializeOptions& options,
                                   const PlaybackParams& params) {
  SaveOp save_op;
  SerializeOp(&save_op, options, params);
}

void PaintOpBufferSerializer::RestoreToCount(
    int count,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  RestoreOp restore_op;
  while (canvas_.getSaveCount() > count) {
    if (!SerializeOp(&restore_op, options, params))
      return;
  }
}

SimpleBufferSerializer::SimpleBufferSerializer(
    void* memory,
    size_t size,
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache)
    : PaintOpBufferSerializer(
          base::Bind(&SimpleBufferSerializer::SerializeToMemory,
                     base::Unretained(this)),
          image_provider,
          transfer_cache),
      memory_(memory),
      total_(size) {}

SimpleBufferSerializer::~SimpleBufferSerializer() = default;

size_t SimpleBufferSerializer::SerializeToMemory(
    const PaintOp* op,
    const PaintOp::SerializeOptions& options) {
  if (written_ == total_)
    return 0u;

  size_t bytes = op->Serialize(static_cast<char*>(memory_) + written_,
                               total_ - written_, options);
  if (!bytes)
    return 0u;

  written_ += bytes;
  DCHECK_GE(total_, written_);
  return bytes;
}

}  // namespace cc
