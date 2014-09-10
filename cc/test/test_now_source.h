// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_NOW_SOURCE_H_
#define CC_TEST_TEST_NOW_SOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/logging.h"

namespace cc {

class TestNowSource : public base::RefCounted<TestNowSource> {
 public:
  static scoped_refptr<TestNowSource> Create();
  static scoped_refptr<TestNowSource> Create(int64_t initial);
  static scoped_refptr<TestNowSource> Create(base::TimeTicks initial);

  virtual void Reset();
  virtual base::TimeTicks Now() const;
  virtual void SetNow(base::TimeTicks time);
  virtual void AdvanceNow(base::TimeDelta period);

  // Convenience functions to make it the now source easier to use in unit
  // tests.
  void AdvanceNowMicroseconds(int64_t period_in_microseconds);
  void SetNowMicroseconds(int64_t time_in_microseconds);

  static const base::TimeTicks kAbsoluteMaxNow;

  // Tracing functions
  scoped_refptr<base::debug::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::debug::TracedValue* state) const;
  std::string ToString() const;

 protected:
  TestNowSource();
  explicit TestNowSource(int64_t initial);
  explicit TestNowSource(base::TimeTicks initial);

  base::TimeTicks initial_;
  base::TimeTicks now_;

 private:
  friend class base::RefCounted<TestNowSource>;
  virtual ~TestNowSource();
};

// gtest pretty printing functions
void PrintTo(const scoped_refptr<TestNowSource>& src, ::std::ostream* os);
::std::ostream& operator<<(::std::ostream& os,
                           const scoped_refptr<TestNowSource>& src);

}  // namespace cc

#endif  // CC_TEST_TEST_NOW_SOURCE_H_
