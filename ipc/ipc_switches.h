// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by the IPC infrastructure.

#ifndef IPC_IPC_SWITCHES_H_
#define IPC_IPC_SWITCHES_H_

#include "base/base_switches.h"

namespace switches {

extern const wchar_t kIPCUseFIFO[];
extern const wchar_t kProcessChannelID[];
extern const wchar_t kDebugChildren[];

}  // namespace switches

#endif  // IPC_IPC_SWITCHES_H_
