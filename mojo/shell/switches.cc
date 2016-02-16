// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"

namespace mojo {
namespace switches {

// Disables the sandbox for debugging.
const char kNoSandbox[] = "no-sandbox";

// If set apps downloaded are saved in with a predictable filename, to help
// remote debugging: when gdb is used through gdbserver, it needs to be able to
// find locally any loaded library. For this, gdb use the filename of the
// library. When using this flag, the application are named with the sha256 of
// their content.
const char kPredictableAppFilenames[] = "predictable-app-filenames";

// Load apps in a single processes.
const char kSingleProcess[] = "single-process";

// Uses the mojo:package_manager application instead of the builtin one.
const char kUseRemotePackageManager[] = "use-remote-package-manager";

}  // namespace switches
}  // namespace mojo
