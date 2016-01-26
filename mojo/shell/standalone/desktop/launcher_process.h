// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_LAUNCHER_PROCESS_H_
#define MOJO_SHELL_STANDALONE_LAUNCHER_PROCESS_H_

#include "base/callback_forward.h"

class GURL;

namespace mojo {
namespace shell {

// Main method for the launcher process.
// See commit in main_helper.h for explanation of the parameters.
int LauncherProcessMain(const GURL& mojo_url, const base::Closure& callback);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_LAUNCHER_PROCESS_H_
