// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/memory_history.h"

namespace cc {

// static
scoped_ptr<MemoryHistory> MemoryHistory::create() {
  return make_scoped_ptr(new MemoryHistory());
}

MemoryHistory::MemoryHistory() {
}

MemoryHistory::Entry MemoryHistory::GetEntry(const size_t& n) const {
  DCHECK(n < ring_buffer_.BufferSize());

  if (ring_buffer_.IsFilledIndex(n))
    return ring_buffer_.ReadBuffer(n);

  return MemoryHistory::Entry();
}

void MemoryHistory::SaveEntry(const MemoryHistory::Entry& entry) {
  ring_buffer_.SaveToBuffer(entry);
}

void MemoryHistory::GetMinAndMax(size_t* min, size_t* max) const {
  *min = std::numeric_limits<size_t>::max();
  *max = 0;

  for (size_t i = 0; i < ring_buffer_.BufferSize(); i++) {
    if (!ring_buffer_.IsFilledIndex(i))
        continue;
    const Entry entry = ring_buffer_.ReadBuffer(i);
    size_t bytes_total = entry.bytes_total();

    if (bytes_total < *min)
      *min = bytes_total;
    if (bytes_total > *max)
        *max = bytes_total;
  }

  if (*min > *max)
    *min = *max;
}

}  // namespace cc
