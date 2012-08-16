// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/values.h>

#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_FILTER_INTERPRETER_H__
#define GESTURES_FILTER_INTERPRETER_H__

namespace gestures {

// Interface for all filter interpreters.

class FilterInterpreter : public Interpreter {
 public:
  FilterInterpreter(PropRegistry* prop_reg, Interpreter* next, Tracer* tracer)
      : Interpreter(prop_reg, tracer) { next_.reset(next); }
  FilterInterpreter(Interpreter* next, Tracer* tracer)
      : Interpreter(tracer) { next_.reset(next); }
  virtual ~FilterInterpreter() {}

  DictionaryValue* EncodeCommonInfo();
  void Clear();

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate,
                                     stime_t* timeout);
  virtual Gesture* HandleTimerImpl(stime_t now, stime_t* timeout);
  virtual void SetHardwarePropertiesImpl(const HardwareProperties& hwprops);

  scoped_ptr<Interpreter> next_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilterInterpreter);
};
}  // namespace gestures

#endif  // GESTURES_FILTER_INTERPRETER_H__
