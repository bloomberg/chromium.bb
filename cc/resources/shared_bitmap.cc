// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/shared_bitmap.h"

#include "base/numerics/safe_math.h"
#include "base/rand_util.h"

namespace cc {

SharedBitmap::SharedBitmap(
    base::SharedMemory* memory,
    const SharedBitmapId& id,
    const base::Callback<void(SharedBitmap*)>& free_callback)
    : memory_(memory), id_(id), free_callback_(free_callback) {}

SharedBitmap::~SharedBitmap() { free_callback_.Run(this); }

// static
bool SharedBitmap::GetSizeInBytes(const gfx::Size& size,
                                  size_t* size_in_bytes) {
  if (size.width() <= 0 || size.height() <= 0)
    return false;
  base::CheckedNumeric<int> s = size.width();
  s *= size.height();
  s *= 4;
  if (!s.IsValid())
    return false;
  *size_in_bytes = s.ValueOrDie();
  return true;
}

// static
SharedBitmapId SharedBitmap::GenerateId() {
  SharedBitmapId id;
  // Needs cryptographically-secure random numbers.
  base::RandBytes(id.name, sizeof(id.name));
  return id;
}

}  // namespace cc
