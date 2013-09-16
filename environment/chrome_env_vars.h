// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENVIRONMENT_CHROME_ENV_VARS_H_
#define ENVIRONMENT_CHROME_ENV_VARS_H_

// This file defines environment variables that are used both by Chrome and
// session_manager.

namespace chromeos {

// File descriptor used to communicate a termination explanation to Breakpad.
const char kBreakpadDescriptorEnvironmentVarName[] = "BREAKPAD_D";

}  // namespace chromeos

#endif  // ENVIRONMENT_CHROME_ENV_VARS_H_
