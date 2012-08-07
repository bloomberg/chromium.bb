// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/values.h>
#include <gtest/gtest.h>

#include "gestures/include/activity_log.h"
#include "gestures/include/gestures.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_INTERPRETER_H__
#define GESTURES_INTERPRETER_H__

// This is a collection of supporting structs and an interface for
// Interpreters.

struct HardwareState;

namespace gestures {

// Interface for all interpreters. Interpreters currently are synchronous.
// A synchronous interpreter will return  0 or 1 Gestures for each passed in
// HardwareState.

class Interpreter {
  FRIEND_TEST(InterpreterTest, ResetLogTest);
  FRIEND_TEST(LoggingFilterInterpreterTest, LogResetHandlerTest);
 public:
  Interpreter(PropRegistry* prop_reg);
  Interpreter();
  virtual ~Interpreter() {}

  // Called to interpret the current state and optionally produce 1
  // resulting gesture. The passed |hwstate| may be modified.
  // If *timeout is set to >0.0, a timer will be setup to call
  // HandleTimer after *timeout time passes. An interpreter can only
  // have up to 1 outstanding timer, so if a timeout is requested by
  // setting *timeout and one already exists, the old one will be cancelled
  // and reused for this timeout.
  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  // Called to handle a timeout and optionally produce 1 resulting gesture.
  // If *timeout is set to >0.0, a timer will be setup to call
  // HandleTimer after *timeout time passes. An interpreter can only
  // have up to 1 outstanding timer, so if a timeout is requested by
  // setting *timeout and one already exists, the old one will be cancelled
  // and reused for this timeout.
  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual DictionaryValue* EncodeCommonInfo();

  std::string Encode();

  virtual void Clear() {
    log_.Clear();
  }

  std::string GetName();

 protected:
  bool logging_enabled_;
  ActivityLog log_;

  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate,
                                 stime_t* timeout) {
    return NULL;
  }
  virtual Gesture* HandleTimerImpl(stime_t now, stime_t* timeout) {
    return NULL;
  }
  virtual void SetHardwarePropertiesImpl(const HardwareProperties& hwprops) {}

 private:
  std::string name_;
  void LogOutputs(Gesture* result, stime_t* timeout);
};
}  // namespace gestures

#endif  // GESTURES_INTERPRETER_H__
