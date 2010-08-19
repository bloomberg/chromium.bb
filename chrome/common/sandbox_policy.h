// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_POLICY_H_
#define CHROME_COMMON_SANDBOX_POLICY_H_
#pragma once

#include "base/process.h"

class CommandLine;
class FilePath;

namespace sandbox {

class BrokerServices;

void InitBrokerServices(sandbox::BrokerServices* broker_services);

// Starts a sandboxed process with the given directory unsandboxed
// and returns a handle to it.
base::ProcessHandle StartProcessWithAccess(CommandLine* cmd_line,
                                           const FilePath& exposed_dir);

}  // namespace sandbox

#endif  // CHROME_COMMON_SANDBOX_POLICY_H_
