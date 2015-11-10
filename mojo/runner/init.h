// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_INIT_H_
#define MOJO_RUNNER_INIT_H_

namespace mojo {
namespace runner {

// Initialization routines shared by desktop and Android main functions.
void InitializeLogging();

void WaitForDebuggerIfNecessary();

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_INIT_H_
