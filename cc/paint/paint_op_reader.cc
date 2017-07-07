// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_reader.h"

#include <stddef.h>

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {

template <typename T>
void PaintOpReader::ReadSimple(T* val) {
  if (remaining_bytes_ < sizeof(T))
    valid_ = false;
  if (!valid_)
    return;

  *val = reinterpret_cast<const T*>(memory_)[0];

  memory_ += sizeof(T);
  remaining_bytes_ -= sizeof(T);
}

void PaintOpReader::ReadData(size_t bytes, void* data) {
  if (remaining_bytes_ < bytes)
    valid_ = false;
  if (!valid_)
    return;
  if (bytes == 0)
    return;

  memcpy(data, memory_, bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::ReadArray(size_t count, SkPoint* array) {
  size_t bytes = count * sizeof(SkPoint);
  if (remaining_bytes_ < bytes)
    valid_ = false;
  // Overflow?
  if (count > static_cast<size_t>(~0) / sizeof(SkPoint))
    valid_ = false;
  if (!valid_)
    return;
  if (count == 0)
    return;

  memcpy(array, memory_, bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::Read(SkScalar* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(size_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(uint8_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(SkRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkIRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkRRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkPath* path) {
  if (!valid_)
    return;

  // TODO(enne): Should the writer write how many bytes it expects as well?
  size_t read_bytes = path->readFromMemory(memory_, remaining_bytes_);
  if (!read_bytes)
    valid_ = false;

  memory_ += read_bytes;
  remaining_bytes_ -= read_bytes;
}

void PaintOpReader::Read(PaintFlags* flags) {
  // TODO(enne): implement PaintFlags serialization: http://crbug.com/737629
}

void PaintOpReader::Read(PaintImage* image) {
  // TODO(enne): implement PaintImage serialization: http://crbug.com/737629
}

void PaintOpReader::Read(sk_sp<SkData>* data) {
  size_t bytes;
  ReadSimple(&bytes);
  if (remaining_bytes_ < bytes)
    valid_ = false;
  if (!valid_)
    return;

  // Separate out empty vs not valid cases.
  if (bytes == 0) {
    bool has_data = false;
    Read(&has_data);
    if (has_data)
      *data = SkData::MakeEmpty();
    return;
  }
  *data = SkData::MakeWithCopy(memory_, bytes);

  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::Read(sk_sp<SkTextBlob>* blob) {
  // TODO(enne): implement SkTextBlob serialization: http://crbug.com/737629
}

}  // namespace cc
