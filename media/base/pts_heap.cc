// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pts_heap.h"

namespace media {

PtsHeap::PtsHeap() {}

PtsHeap::~PtsHeap() {}

void PtsHeap::Push(const base::TimeDelta& pts) {
  queue_.push(pts);
}

void PtsHeap::Pop() {
  queue_.pop();
}

}  // namespace media
