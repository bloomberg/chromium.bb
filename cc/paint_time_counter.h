// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TIME_COUNTER_H_
#define CC_PAINT_TIME_COUNTER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/ring_buffer.h"

namespace cc {

// Maintains a history of paint times for each frame
class PaintTimeCounter {
 public:
  static scoped_ptr<PaintTimeCounter> create();

  size_t HistorySize() const { return ring_buffer_.BufferSize(); }

  // n = 0 returns the oldest and
  // n = PaintTimeHistorySize() - 1 the most recent paint time.
  base::TimeDelta GetPaintTimeOfRecentFrame(const size_t& n) const;

  void SavePaintTime(const base::TimeDelta& total_paint_time);
  void GetMinAndMaxPaintTime(base::TimeDelta* min, base::TimeDelta* max) const;

 private:
  PaintTimeCounter();

  RingBuffer<base::TimeDelta, 80> ring_buffer_;
  base::TimeDelta last_total_paint_time_;

  DISALLOW_COPY_AND_ASSIGN(PaintTimeCounter);
};

}  // namespace cc

#endif  // CC_PAINT_TIME_COUNTER_H_
