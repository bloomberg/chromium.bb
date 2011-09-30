// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_RUNTIME_ENVIRONMENT_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_RUNTIME_ENVIRONMENT_H_
#pragma once

namespace chromeos {
namespace system {
namespace runtime_environment {

// Returns true if the browser is running on Chrome OS.
// Useful for implementing stubs for Linux desktop.
bool IsRunningOnChromeOS();

}  // namespace runtime_environment
}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_RUNTIME_ENVIRONMENT_H_
