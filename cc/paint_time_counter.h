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

  struct Entry {
    Entry()
        : commit_number(0) { }

    int commit_number;
    base::TimeDelta paint_time;
    base::TimeDelta rasterize_time;

    base::TimeDelta total_time() const {
      return paint_time + rasterize_time;
    }
  };

  // n = 0 returns the oldest and
  // n = PaintTimeHistorySize() - 1 the most recent paint time.
  base::TimeDelta GetPaintTimeOfRecentFrame(const size_t& n) const;

  void SavePaintTime(const base::TimeDelta& total_paint_time,
                     int commit_number);
  void SaveRasterizeTime(const base::TimeDelta& total_rasterize_time,
                         int commit_number);
  void GetMinAndMaxPaintTime(base::TimeDelta* min, base::TimeDelta* max) const;

  void ClearHistory();

  typedef RingBuffer<Entry, 90> RingBufferType;
  RingBufferType::Iterator Begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator End() const { return ring_buffer_.End(); }

 private:
  PaintTimeCounter();

  RingBufferType ring_buffer_;

  base::TimeDelta last_total_paint_time_;
  base::TimeDelta last_total_rasterize_time_;

  bool can_save_paint_time_delta_;
  bool can_save_rasterize_time_delta_;

  DISALLOW_COPY_AND_ASSIGN(PaintTimeCounter);
};

}  // namespace cc

#endif  // CC_PAINT_TIME_COUNTER_H_
