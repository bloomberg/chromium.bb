// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_command_line_override.h"

ScopedCommandLineOverride::ScopedCommandLineOverride(
    const std::string& new_switch)
    : old_command_line_(*CommandLine::ForCurrentProcess()) {
  CommandLine::ForCurrentProcess()->AppendSwitch(new_switch);
}

ScopedCommandLineOverride::~ScopedCommandLineOverride() {
  *CommandLine::ForCurrentProcess() = old_command_line_;
}
