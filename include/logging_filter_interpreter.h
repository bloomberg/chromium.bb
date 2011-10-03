// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

#include "gestures/include/activity_log.h"
#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_LOGGING_FILTER_INTERPRETER_H_
#define GESTURES_LOGGING_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter keeps an ActivityLog of everything that happens, and can
// log it when requested.

class LoggingFilterInterpreter : public Interpreter {
  FRIEND_TEST(LoggingFilterInterpreterTest, SimpleTest);
 public:
  // Takes ownership of |next|:
  explicit LoggingFilterInterpreter(Interpreter* next);
  virtual ~LoggingFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

 private:
  scoped_ptr<Interpreter> next_;

  void LogOutputs(Gesture* result, stime_t* timeout);

  static GesturesPropBool StaticLoggingNotifyGet(void* unused) { return 0; }
  static void StaticLoggingNotifySet(void* data) {
    reinterpret_cast<LoggingFilterInterpreter*>(data)->LoggingNotifySet();
  }
  void LoggingNotifySet();

  ActivityLog log_;

  int logging_notify_;
  GesturesProp* logging_notify_prop_;
};

}  // namespace gestures

#endif  // GESTURES_LOGGING_FILTER_INTERPRETER_H_
