// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_switches.h"

#include "base/base_switches.h"

namespace switches {

// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.

// On POSIX only: use FIFO for IPC channels so that "unrelated" process
// can connect to a channel, provided it knows its name. For debugging purposes.
const wchar_t kIPCUseFIFO[]                    = L"ipc-use-fifo";

// The value of this switch tells the child process which
// IPC channel the browser expects to use to communicate with it.
const wchar_t kProcessChannelID[]              = L"channel";

// Will add kDebugOnStart to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kDebugOnStart flag passed on or not.
const wchar_t kDebugChildren[]                 = L"debug-children";

}  // namespace switches

