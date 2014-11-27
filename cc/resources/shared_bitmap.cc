// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/shared_bitmap.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"

namespace cc {

SharedBitmap::SharedBitmap(uint8* pixels, const SharedBitmapId& id)
    : pixels_(pixels), id_(id) {
}

SharedBitmap::~SharedBitmap() {
}

// static
bool SharedBitmap::SizeInBytes(const gfx::Size& size, size_t* size_in_bytes) {
  if (size.IsEmpty())
    return false;
  base::CheckedNumeric<size_t> s = 4;
  s *= size.width();
  s *= size.height();
  if (!s.IsValid())
    return false;
  *size_in_bytes = s.ValueOrDie();
  return true;
}

// static
size_t SharedBitmap::CheckedSizeInBytes(const gfx::Size& size) {
  CHECK(!size.IsEmpty());
  base::CheckedNumeric<size_t> s = 4;
  s *= size.width();
  s *= size.height();
  return s.ValueOrDie();
}

// static
size_t SharedBitmap::UncheckedSizeInBytes(const gfx::Size& size) {
  DCHECK(VerifySizeInBytes(size));
  size_t s = 4;
  s *= size.width();
  s *= size.height();
  return s;
}

// static
bool SharedBitmap::VerifySizeInBytes(const gfx::Size& size) {
  if (size.IsEmpty())
    return false;
  base::CheckedNumeric<size_t> s = 4;
  s *= size.width();
  s *= size.height();
  return s.IsValid();
}

// static
SharedBitmapId SharedBitmap::GenerateId() {
  SharedBitmapId id;
  // Needs cryptographically-secure random numbers.
  base::RandBytes(id.name, sizeof(id.name));
  return id;
}

}  // namespace cc
