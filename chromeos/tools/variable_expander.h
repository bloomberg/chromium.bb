// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TOOLS_VARIABLE_EXPANDER_H_
#define CHROMEOS_TOOLS_VARIABLE_EXPANDER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include "chromeos/chromeos_export.h"

namespace base {
class Value;
}

namespace chromeos {

// Expands variables in a string or base::Value. Allows setting which variables
// to expand and the corresponding values. Also supports substrings with given
// zero-based position or position/count, e.g. if the variable name is
// "machine_name" and the value is "chromebook", then the this class expands
//   "${machine_name}"     to "chromebook",
//   "${machine_name,6}"   to "book" (position 6 of "chromebook" to end) and
//   "${machine_name,2,4}" to "rome" (4 characters from position 2).
class CHROMEOS_EXPORT VariableExpander {
 public:
  VariableExpander();

  // Takes a map of variables to values.
  explicit VariableExpander(std::map<std::string, std::string> variables);

  ~VariableExpander();

  // Sets a |variable| to expand and the corresponding |value|.
  void SetVariable(const std::string& variable, const std::string& value);

  // Expands all variables in |str|, see SetVariable(). Returns true if no error
  // has occurred. Returns false if at least one variable was malformed and
  // could not be expanded (the good ones are still expanded).
  bool ExpandString(std::string* str);

  // Calls ExpandString on every string contained in |value|. Recursively
  // handles all hierarchy levels. Returns true if no error has occurred.
  // Returns false if at least one variable was malformed and could not be
  // expanded (the good ones are still expanded).
  bool ExpandValue(base::Value* value);

 private:
  // Maps variable -> value.
  std::map<std::string, std::string> variables_;

  DISALLOW_COPY_AND_ASSIGN(VariableExpander);
};

}  // namespace chromeos

#endif  // CHROMEOS_TOOLS_VARIABLE_EXPANDER_H_
