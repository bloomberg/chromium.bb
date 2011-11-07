// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_COMMAND_LINE_OVERRIDE_H_
#define CHROME_TEST_BASE_SCOPED_COMMAND_LINE_OVERRIDE_H_
#pragma once

#include <string>

#include "base/command_line.h"

// ScopedCommandLineOverride takes the CommandLine for the current
// process, saves it, then alters it. When the instance of this class
// goes out of scope, it restores CommandLine to its prior state. The
// intended usage is to test something that requires a flag that's off
// by default:
//
// ASSERT_TRUE(!Something::NotFullyBaked());
// {
//   ScopedCommandLineOverride override(switches:kEnableSomethingHalfBaked);
//   ASSERT_TRUE(Something::NotFullyBaked());
// }
class ScopedCommandLineOverride {
 public:
  explicit ScopedCommandLineOverride(const std::string& new_switch);
  virtual ~ScopedCommandLineOverride();
 private:
  CommandLine old_command_line_;
};

#endif  // CHROME_TEST_BASE_SCOPED_COMMAND_LINE_OVERRIDE_H_
