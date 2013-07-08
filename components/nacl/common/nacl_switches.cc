// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_switches.h"

namespace switches {

// Enables debugging via RSP over a socket.
const char kEnableNaClDebug[]               = "enable-nacl-debug";

// Causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// Uses NaCl manifest URL to choose whether NaCl program will be debugged by
// debug stub.
// Switch value format: [!]pattern1,pattern2,...,patternN. Each pattern uses
// the same syntax as patterns in Chrome extension manifest. The only difference
// is that * scheme matches all schemes instead of matching only http and https.
// If the value doesn't start with !, a program will be debugged if manifest URL
// matches any pattern. If the value starts with !, a program will be debugged
// if manifest URL does not match any pattern.
const char kNaClDebugMask[]                 = "nacl-debug-mask";

// Native Client GDB debugger that will be launched automatically when needed.
const char kNaClGdb[]                       = "nacl-gdb";

// GDB script to pass to the nacl-gdb debugger at startup.
const char kNaClGdbScript[]                 = "nacl-gdb-script";

// On POSIX only: the contents of this flag are prepended to the nacl-loader
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kNaClLoaderCmdPrefix[]           = "nacl-loader-cmd-prefix";

// Causes the process to run as a NativeClient loader.
const char kNaClLoaderProcess[]             = "nacl-loader";

// Runs the security test for the NaCl loader sandbox.
const char kTestNaClSandbox[]               = "test-nacl-sandbox";

}  // namespace switches
