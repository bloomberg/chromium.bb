// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_WRITER_H_
#define CC_PAINT_PAINT_OP_WRITER_H_

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"

struct SkRect;
struct SkIRect;
class SkRRect;

namespace cc {

class ImageDecodeCache;
class PaintShader;

class CC_PAINT_EXPORT PaintOpWriter {
 public:
  PaintOpWriter(void* memory, size_t size)
      : memory_(static_cast<char*>(memory) + HeaderBytes()),
        size_(size),
        remaining_bytes_(size - HeaderBytes()) {
    // Leave space for header of type/skip.
    DCHECK_GE(size, HeaderBytes());
  }

  static size_t constexpr HeaderBytes() { return 4u; }

  // Write a sequence of arbitrary bytes.
  void WriteData(size_t bytes, const void* input);

  void WriteArray(size_t count, const SkPoint* input);

  size_t size() const { return valid_ ? size_ - remaining_bytes_ : 0u; }

  void Write(SkScalar data);
  void Write(size_t data);
  void Write(uint8_t data);
  void Write(const SkRect& rect);
  void Write(const SkIRect& rect);
  void Write(const SkRRect& rect);

  void Write(const SkPath& path);
  void Write(const PaintFlags& flags);
  void Write(const PaintImage& image, ImageDecodeCache* cache);
  void Write(const sk_sp<SkData>& data);
  void Write(const sk_sp<SkTextBlob>& blob);
  void Write(const PaintShader* shader);

  void Write(SkClipOp op) { Write(static_cast<uint8_t>(op)); }
  void Write(PaintCanvas::AnnotationType type) {
    Write(static_cast<uint8_t>(type));
  }
  void Write(PaintCanvas::SrcRectConstraint constraint) {
    Write(static_cast<uint8_t>(constraint));
  }
  void Write(bool data) { Write(static_cast<uint8_t>(data)); }

 private:
  template <typename T>
  void WriteSimple(const T& val);

  void WriteFlattenable(const SkFlattenable* val);

  // Attempts to align the memory to the given alignment. Returns false if there
  // is unsufficient bytes remaining to do this padding.
  bool AlignMemory(size_t alignment);

  char* memory_ = nullptr;
  size_t size_ = 0u;
  size_t remaining_bytes_ = 0u;
  bool valid_ = true;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_WRITER_H_
