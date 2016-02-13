// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SWITCHES_H_
#define MOJO_SHELL_SWITCHES_H_

#include <set>
#include <string>

namespace mojo {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kDontDeleteOnDownload[];
extern const char kMojoNoSandbox[];
extern const char kMojoSingleProcess[];
extern const char kPredictableAppFilenames[];
extern const char kUseUpdater[];

}  // namespace switches
}  // namespace mojo

#endif  // MOJO_SHELL_SWITCHES_H_
