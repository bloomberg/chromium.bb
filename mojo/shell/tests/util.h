// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TESTS_UTIL_H_
#define MOJO_SHELL_TESTS_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class Process;
}

namespace mojo {
class Connection;
class Connector;
class Identity;
namespace shell {
namespace test {

// Starts the process @ |target_exe_name| and connects to it as |target| using
// |connector|, returning the connection & the process.
// This blocks until the connection is established/rejected by the shell.
scoped_ptr<Connection> LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity target,
    mojo::Connector* connector,
    base::Process* process);

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TESTS_UTIL_H_
