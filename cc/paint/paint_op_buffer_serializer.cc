// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer_serializer.h"

#include "base/bind.h"
#include "cc/paint/scoped_image_flags.h"
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

PaintOpBufferSerializer::PaintOpBufferSerializer(SerializeCallback serialize_cb,
                                                 ImageProvider* image_provider)
    : serialize_cb_(std::move(serialize_cb)),
      canvas_(100, 100),
      image_provider_(image_provider) {
  DCHECK(serialize_cb_);
}

PaintOpBufferSerializer::~PaintOpBufferSerializer() = default;

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer,
                                        const std::vector<size_t>* offsets,
                                        const Preamble& preamble) {
  PaintOp::SerializeOptions options(image_provider_, &canvas_,
                                    canvas_.getTotalMatrix());
  PlaybackParams params(image_provider_, canvas_.getTotalMatrix());

  int save_count = canvas_.getSaveCount();
  Save(options, params);
  SerializePreamble(preamble, options, params);
  SerializeBuffer(buffer, offsets);
  RestoreToCount(save_count, options, params);
}

void PaintOpBufferSerializer::SerializePreamble(
    const Preamble& preamble,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  if (!preamble.translation.IsZero()) {
    TranslateOp translate_op(-preamble.translation.x(),
                             -preamble.translation.y());
    SerializeOp(&translate_op, options, params);
  }

  if (!preamble.playback_rect.IsEmpty()) {
    ClipRectOp clip_op(
        SkRect::MakeFromIRect(gfx::RectToSkIRect(preamble.playback_rect)),
        SkClipOp::kIntersect, false);
    SerializeOp(&clip_op, options, params);
  }

  if (!preamble.post_translation.IsZero()) {
    TranslateOp translate_op(preamble.post_translation.x(),
                             preamble.post_translation.y());
    SerializeOp(&translate_op, options, params);
  }

  if (preamble.post_scale != 1.f) {
    ScaleOp scale_op(preamble.post_scale, preamble.post_scale);
    SerializeOp(&scale_op, options, params);
  }
}

void PaintOpBufferSerializer::SerializeBuffer(
    const PaintOpBuffer* buffer,
    const std::vector<size_t>* offsets) {
  PaintOp::SerializeOptions options(image_provider_, &canvas_,
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
  const bool needs_flag_override =
      alpha != 255 ||
      (flags_op->flags.HasDiscardableImages() && options->image_provider);

  base::Optional<ScopedImageFlags> scoped_flags;
  if (needs_flag_override) {
    scoped_flags.emplace(options->image_provider, &flags_op->flags,
                         options->canvas->getTotalMatrix(), alpha);
  }

  const PaintFlags* flags_to_serialize =
      scoped_flags ? scoped_flags->flags() : &flags_op->flags;
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

SimpleBufferSerializer::SimpleBufferSerializer(void* memory,
                                               size_t size,
                                               ImageProvider* image_provider)
    : PaintOpBufferSerializer(
          base::Bind(&SimpleBufferSerializer::SerializeToMemory,
                     base::Unretained(this)),
          image_provider),
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
