// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/shared_bitmap.h"

namespace cc {

SharedBitmap::SharedBitmap(
    base::SharedMemory* memory,
    const SharedBitmapId& id,
    const base::Callback<void(SharedBitmap*)>& free_callback)
    : memory_(memory), id_(id), free_callback_(free_callback) {}

SharedBitmap::~SharedBitmap() { free_callback_.Run(this); }

}  // namespace cc
