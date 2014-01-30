// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_
#define MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {

class CongestionControl {
 public:
  CongestionControl(base::TickClock* clock,
                    float congestion_control_back_off,
                    uint32 max_bitrate_configured,
                    uint32 min_bitrate_configured,
                    uint32 start_bitrate);

  virtual ~CongestionControl();

  // Don't call OnAck if the same message contain a NACK.
  // Returns true if the bitrate have changed.
  bool OnAck(base::TimeDelta rtt_ms, uint32* new_bitrate);

  // Returns true if the bitrate have changed.
  bool OnNack(base::TimeDelta rtt_ms, uint32* new_bitrate);

 private:
  base::TickClock* const clock_;  // Not owned by this class.
  const float congestion_control_back_off_;
  const uint32 max_bitrate_configured_;
  const uint32 min_bitrate_configured_;
  uint32 bitrate_;
  base::TimeTicks time_last_increase_;
  base::TimeTicks time_last_decrease_;

  DISALLOW_COPY_AND_ASSIGN(CongestionControl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_
