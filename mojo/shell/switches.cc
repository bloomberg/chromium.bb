// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"


namespace switches {

// If set apps downloaded are not deleted.
const char kDontDeleteOnDownload[] = "dont-delete-on-download";

// Load apps in separate processes.
// TODO(vtl): Work in progress; doesn't work. Flip this to "disable" (or maybe
// change it to "single-process") when it works.
const char kEnableMultiprocess[] = "enable-multiprocess";

// Disables the sandbox for debugging. (Why the Mojo prefix on the constant?
// Because otherwise we conflict with content.)
const char kMojoNoSandbox[] = "no-sandbox";

// Load apps in a single processes.
const char kMojoSingleProcess[] = "single-process";

// If set apps downloaded are saved in with a predictable filename, to help
// remote debugging: when gdb is used through gdbserver, it needs to be able to
// find locally any loaded library. For this, gdb use the filename of the
// library. When using this flag, the application are named with the sha256 of
// their content.
const char kPredictableAppFilenames[] = "predictable-app-filenames";

// Pull apps via component updater, rather than using default local resolution
// to find them.
const char kUseUpdater[] = "use-updater";

}  // namespace switches
