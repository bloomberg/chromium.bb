// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_READER_H_
#define CC_PAINT_PAINT_OP_READER_H_

#include <vector>

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_writer.h"

namespace cc {

class PaintShader;

// PaintOpReader takes garbage |memory| and clobbers it with successive
// read functions.
class CC_PAINT_EXPORT PaintOpReader {
 public:
  PaintOpReader(const volatile void* memory, size_t size)
      : memory_(static_cast<const volatile char*>(memory) +
                PaintOpWriter::HeaderBytes()),
        remaining_bytes_(size - PaintOpWriter::HeaderBytes()) {
    if (size < PaintOpWriter::HeaderBytes())
      valid_ = false;
  }

  static void FixupMatrixPostSerialization(SkMatrix* matrix);
  static bool ReadAndValidateOpHeader(const volatile void* input,
                                      size_t input_size,
                                      uint8_t* type,
                                      uint32_t* skip);

  bool valid() const { return valid_; }

  void ReadData(size_t bytes, void* data);
  void ReadArray(size_t count, SkPoint* array);

  void Read(SkScalar* data);
  void Read(size_t* data);
  void Read(uint8_t* data);
  void Read(SkRect* rect);
  void Read(SkIRect* rect);
  void Read(SkRRect* rect);

  void Read(SkPath* path);
  void Read(PaintFlags* flags);
  void Read(PaintImage* image);
  void Read(sk_sp<SkData>* data);
  void Read(scoped_refptr<PaintTextBlob>* blob);
  void Read(sk_sp<PaintShader>* shader);
  void Read(SkMatrix* matrix);

  void Read(SkClipOp* op) {
    uint8_t value = 0u;
    Read(&value);
    *op = static_cast<SkClipOp>(value);
  }
  void Read(PaintCanvas::AnnotationType* type) {
    uint8_t value = 0u;
    Read(&value);
    *type = static_cast<PaintCanvas::AnnotationType>(value);
  }
  void Read(PaintCanvas::SrcRectConstraint* constraint) {
    uint8_t value = 0u;
    Read(&value);
    *constraint = static_cast<PaintCanvas::SrcRectConstraint>(value);
  }
  void Read(bool* data) {
    uint8_t value = 0u;
    Read(&value);
    *data = !!value;
  }

 private:
  template <typename T>
  void ReadSimple(T* val);

  template <typename T>
  void ReadFlattenable(sk_sp<T>* val);

  void Read(std::vector<PaintTypeface>* typefaces);
  void Read(const std::vector<PaintTypeface>& typefaces,
            sk_sp<SkTextBlob>* blob);

  void SetInvalid();

  // Attempts to align the memory to the given alignment. Returns false if there
  // is unsufficient bytes remaining to do this padding.
  bool AlignMemory(size_t alignment);

  const volatile char* memory_ = nullptr;
  size_t remaining_bytes_ = 0u;
  bool valid_ = true;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_READER_H_
