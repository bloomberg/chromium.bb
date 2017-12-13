// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
#define CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_

#include "cc/paint/paint_op_buffer.h"

#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class TransferCacheSerializeHelper;
class CC_PAINT_EXPORT PaintOpBufferSerializer {
 public:
  using SerializeCallback =
      base::Callback<size_t(const PaintOp*, const PaintOp::SerializeOptions&)>;

  PaintOpBufferSerializer(SerializeCallback serialize_cb,
                          ImageProvider* image_provider,
                          TransferCacheSerializeHelper* transfer_cache);
  virtual ~PaintOpBufferSerializer();

  struct Preamble {
    gfx::Vector2dF translation;
    gfx::Rect playback_rect;
    gfx::Vector2dF post_translation;
    float post_scale = 1.f;
  };
  void Serialize(const PaintOpBuffer* buffer,
                 const std::vector<size_t>* offsets,
                 const Preamble& preamble);

  bool valid() const { return valid_; }

 private:
  void SerializePreamble(const Preamble& preamble,
                         const PaintOp::SerializeOptions& options,
                         const PlaybackParams& params);
  void SerializeBuffer(const PaintOpBuffer* buffer,
                       const std::vector<size_t>* offsets);
  bool SerializeOpWithFlags(const PaintOpWithFlags* flags_op,
                            PaintOp::SerializeOptions* options,
                            const PlaybackParams& params,
                            uint8_t alpha);
  bool SerializeOp(const PaintOp* op,
                   const PaintOp::SerializeOptions& options,
                   const PlaybackParams& params);
  void Save(const PaintOp::SerializeOptions& options,
            const PlaybackParams& params);
  void RestoreToCount(int count,
                      const PaintOp::SerializeOptions& options,
                      const PlaybackParams& params);

  SerializeCallback serialize_cb_;
  SkNoDrawCanvas canvas_;
  ImageProvider* image_provider_;
  TransferCacheSerializeHelper* transfer_cache_;
  bool valid_ = true;
};

// Serializes the ops in the memory available, fails on overflow.
class CC_PAINT_EXPORT SimpleBufferSerializer : public PaintOpBufferSerializer {
 public:
  SimpleBufferSerializer(void* memory,
                         size_t size,
                         ImageProvider* image_provider,
                         TransferCacheSerializeHelper* transfer_cache);
  ~SimpleBufferSerializer() override;

  size_t written() const { return written_; }

 private:
  size_t SerializeToMemory(const PaintOp* op,
                           const PaintOp::SerializeOptions& options);

  void* memory_;
  const size_t total_;
  size_t written_ = 0u;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
