// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_POLICY_H_
#define CONTENT_COMMON_SANDBOX_POLICY_H_
#pragma once

#include "base/process.h"
#include "content/common/content_export.h"

class CommandLine;
class FilePath;

namespace sandbox {

class BrokerServices;
class TargetServices;

bool InitBrokerServices(sandbox::BrokerServices* broker_services);

bool InitTargetServices(sandbox::TargetServices* target_services);

// Starts a sandboxed process with the given directory unsandboxed
// and returns a handle to it.
base::ProcessHandle StartProcessWithAccess(CommandLine* cmd_line,
                                           const FilePath& exposed_dir);

}  // namespace sandbox

#endif  // CONTENT_COMMON_SANDBOX_POLICY_H_
