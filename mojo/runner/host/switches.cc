// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/host/switches.h"

namespace switches {

// Used internally by the main process to indicate that a new process should be
// a child process. Takes the absolute path to the mojo application to load as
// an argument. Not for user use.
const char kChildProcess[] = "child-process";

// Enables the sandbox on this process.
const char kEnableSandbox[] = "enable-sandbox";

}  // namespace switches
