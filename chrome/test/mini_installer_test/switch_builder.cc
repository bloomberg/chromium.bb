// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/switch_builder.h"

#include "chrome/installer/util/install_util.h"

namespace installer_test {

SwitchBuilder::SwitchBuilder()
    : switches_(CommandLine::NO_PROGRAM) {}

SwitchBuilder::~SwitchBuilder() {}

const CommandLine& SwitchBuilder::GetSwitches() const {
  return switches_;
}

SwitchBuilder& SwitchBuilder::AddChrome() {
  switches_.AppendSwitch(installer::switches::kChrome);
  return *this;
}

SwitchBuilder& SwitchBuilder::AddChromeFrame() {
  switches_.AppendSwitch(installer::switches::kChromeFrame);
  switches_.AppendSwitch(installer::switches::kDoNotLaunchChrome);
  switches_.AppendSwitch(installer::switches::kDoNotRegisterForUpdateLaunch);
  return *this;
}

SwitchBuilder& SwitchBuilder::AddMultiInstall() {
  switches_.AppendSwitch(installer::switches::kMultiInstall);
  return *this;
}

SwitchBuilder& SwitchBuilder::AddReadyMode() {
  switches_.AppendSwitch(installer::switches::kChromeFrameReadyMode);
  return *this;
}

SwitchBuilder& SwitchBuilder::AddSystemInstall() {
  switches_.AppendSwitch(installer::switches::kSystemLevel);
  return *this;
}

}  // namespace
