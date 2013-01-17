// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint_time_counter.h"

namespace cc {

// static
scoped_ptr<PaintTimeCounter> PaintTimeCounter::create() {
  return make_scoped_ptr(new PaintTimeCounter());
}

PaintTimeCounter::PaintTimeCounter() {
}

base::TimeDelta PaintTimeCounter::GetPaintTimeOfRecentFrame(
    const size_t& n) const {
  DCHECK(n < ring_buffer_.BufferSize());

  if (ring_buffer_.IsFilledIndex(n))
    return ring_buffer_.ReadBuffer(n);

  return base::TimeDelta();
}

void PaintTimeCounter::SavePaintTime(const base::TimeDelta& total_paint_time) {
  base::TimeDelta paint_time = total_paint_time - last_total_paint_time_;

  if (paint_time.InMillisecondsF() > 0)
    ring_buffer_.SaveToBuffer(paint_time);

  last_total_paint_time_ = total_paint_time;
}

void PaintTimeCounter::GetMinAndMaxPaintTime(base::TimeDelta* min,
                                             base::TimeDelta* max) const {
  *min = base::TimeDelta::FromDays(1);
  *max = base::TimeDelta();

  for (size_t i = 0; i < ring_buffer_.BufferSize(); i++) {
    if (ring_buffer_.IsFilledIndex(i)) {
      base::TimeDelta paint_time = ring_buffer_.ReadBuffer(i);

      if (paint_time < *min)
        *min = paint_time;
      if (paint_time > *max)
        *max = paint_time;
    }
  }

  if (*min > *max)
    *min = *max;
}

}  // namespace cc
