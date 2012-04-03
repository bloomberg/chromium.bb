// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IE configurator mutates Internet Explorer's configuration to put it in a
// (thought to be) known good state for the Chrome Frame tests.  The current
// user's IE configuration may be left in a different state than it was
// initially; see the implementation for details.

#ifndef CHROME_FRAME_TEST_IE_CONFIGURATOR_H_
#define CHROME_FRAME_TEST_IE_CONFIGURATOR_H_
#pragma once

#include "base/basictypes.h"

namespace chrome_frame_test {

// Abstract interface to be implemented for per-version configurators.
class IEConfigurator {
 public:
  virtual ~IEConfigurator();

  // Initializes a configurator, causing it to cache existing configuration
  // settings that it will modify.
  virtual void Initialize() = 0;

  // Applies all configuration settings.
  virtual void ApplySettings() = 0;

  // Reverts all configuration settings.
  virtual void RevertSettings() = 0;

 protected:
  IEConfigurator();
};

// Returns a new configurator for the current configuration, or NULL if none
// applies.
IEConfigurator* CreateConfigurator();

// Installs a configurator in the Google Test unit test singleton.
void InstallIEConfigurator();

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_IE_CONFIGURATOR_H_
