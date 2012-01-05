// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_SWITCH_BUILDER_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_SWITCH_BUILDER_H_
#pragma once

#include "base/basictypes.h"
#include "base/command_line.h"

namespace installer_test {

// Builds commandline arguments for Chrome installation.
class SwitchBuilder {
 public:
  SwitchBuilder();
  ~SwitchBuilder();

  const CommandLine& GetSwitches() const;

  SwitchBuilder& AddChrome();
  SwitchBuilder& AddChromeFrame();
  SwitchBuilder& AddMultiInstall();
  SwitchBuilder& AddReadyMode();
  SwitchBuilder& AddSystemInstall();

 private:
  CommandLine switches_;
  DISALLOW_COPY_AND_ASSIGN(SwitchBuilder);
};

}  // namespace

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_SWITCH_BUILDER_H_
