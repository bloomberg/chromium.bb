// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QuicTime represents one point in time, stored in microsecond resolution.
// QuicTime is monotonically increasing, even across system clock adjustments.
// The epoch (time 0) of QuicTime is unspecified.
//
// This implementation wraps the classes base::TimeTicks and base::TimeDelta.

#ifndef NET_QUIC_QUIC_TIME_H_
#define NET_QUIC_QUIC_TIME_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE QuicTime {
 public:
  // A QuicTime::Delta represents the signed difference between two points in
  // time, stored in microsecond resolution.
  class NET_EXPORT_PRIVATE Delta {
   public:
    explicit Delta(base::TimeDelta delta);

    // Create a object with an offset of 0.
    static Delta Zero();

    // Create a object with infinite offset time.
    static Delta Infinite();

    // Converts a number of seconds to a time offset.
    static Delta FromSeconds(int64 secs);

    // Converts a number of milliseconds to a time offset.
    static Delta FromMilliseconds(int64 ms);

    // Converts a number of microseconds to a time offset.
    static Delta FromMicroseconds(int64 us);

    // Converts the time offset to a rounded number of seconds.
    int64 ToSeconds() const;

    // Converts the time offset to a rounded number of milliseconds.
    int64 ToMilliseconds() const;

    // Converts the time offset to a rounded number of microseconds.
    int64 ToMicroseconds() const;

    Delta Add(const Delta& delta) const;

    Delta Subtract(const Delta& delta) const;

    bool IsZero() const;

    bool IsInfinite() const;

   private:
    base::TimeDelta delta_;

    friend class QuicTime;
    friend class QuicClock;
  };

  explicit QuicTime(base::TimeTicks ticks);

  // Creates a new QuicTime with an internal value of 0.  IsInitialized()
  // will return false for these times.
  static QuicTime Zero();

  // Produce the internal value to be used when logging.  This value
  // represents the number of microseconds since some epoch.  It may
  // be the UNIX epoch on some platforms.  On others, it may
  // be a CPU ticks based value.
  int64 ToDebuggingValue() const;

  bool IsInitialized() const;

  QuicTime Add(const Delta& delta) const;

  QuicTime Subtract(const Delta& delta) const;

  Delta Subtract(const QuicTime& other) const;

 private:
  friend bool operator==(QuicTime lhs, QuicTime rhs);
  friend bool operator<(QuicTime lhs, QuicTime rhs);

  friend class QuicClock;
  friend class QuicClockTest;

  base::TimeTicks ticks_;
};

// Non-member relational operators for QuicTime::Delta.
inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.ToMicroseconds() == rhs.ToMicroseconds();
}
inline bool operator!=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.ToMicroseconds() < rhs.ToMicroseconds();
}
inline bool operator>(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs < rhs);
}
// Non-member relational operators for QuicTime.
inline bool operator==(QuicTime lhs, QuicTime rhs) {
  return lhs.ticks_ == rhs.ticks_;
}
inline bool operator!=(QuicTime lhs, QuicTime rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime lhs, QuicTime rhs) {
  return lhs.ticks_ < rhs.ticks_;
}
inline bool operator>(QuicTime lhs, QuicTime rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime lhs, QuicTime rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime lhs, QuicTime rhs) {
  return !(lhs < rhs);
}

}  // namespace net

#endif  // NET_QUIC_QUIC_TIME_H_
