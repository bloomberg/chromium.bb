// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

#include "gestures/include/activity_log.h"
#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_LOGGING_FILTER_INTERPRETER_H_
#define GESTURES_LOGGING_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter keeps an ActivityLog of everything that happens, and can
// log it when requested.

class LoggingFilterInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(ActivityReplayTest, DISABLED_SimpleTest);
  FRIEND_TEST(LoggingFilterInterpreterTest, SimpleTest);
 public:
  // Takes ownership of |next|:
  LoggingFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~LoggingFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void IntWasWritten(IntProperty* prop);

 private:
  scoped_ptr<Interpreter> next_;

  void LogOutputs(Gesture* result, stime_t* timeout);

  ActivityLog log_;

  IntProperty logging_notify_;
};

}  // namespace gestures

#endif  // GESTURES_LOGGING_FILTER_INTERPRETER_H_
