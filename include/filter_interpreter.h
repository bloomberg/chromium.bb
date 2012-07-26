// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/values.h>

#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_FILTER_INTERPRETER_H__
#define GESTURES_FILTER_INTERPRETER_H__

namespace gestures {

// Interface for all filter interpreters.

class FilterInterpreter : public Interpreter {
 public:
  FilterInterpreter(PropRegistry* prop_reg)
    : Interpreter(prop_reg) {}
  FilterInterpreter() : Interpreter() {}

  virtual ~FilterInterpreter() {}

  DictionaryValue* EncodeCommonInfo();

  void Clear();

 protected:
  scoped_ptr<Interpreter> next_;
};
}  // namespace gestures

#endif  // GESTURES_FILTER_INTERPRETER_H__
