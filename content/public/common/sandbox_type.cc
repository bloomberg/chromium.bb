// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_type.h"

#include <string>

#include "content/public/common/content_switches.h"

namespace content {

SandboxType SandboxTypeFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return SANDBOX_TYPE_NO_SANDBOX;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kRendererProcess)
    return SANDBOX_TYPE_RENDERER;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kUtilityProcessSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return SANDBOX_TYPE_NO_SANDBOX;
    return SANDBOX_TYPE_GPU;
  }
  if (process_type == switches::kPpapiBrokerProcess)
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kPpapiPluginProcess)
    return SANDBOX_TYPE_PPAPI;

  // This is a process which we don't know about, i.e. an embedder-defined
  // process. If the embedder wants it sandboxed, they have a chance to return
  // the sandbox profile in ContentClient::GetSandboxProfileForSandboxType.
  return SANDBOX_TYPE_INVALID;
}

SandboxType UtilitySandboxTypeFromString(const std::string& sandbox_string) {
  if (sandbox_string == "none")
    return SANDBOX_TYPE_NO_SANDBOX;
  if (sandbox_string == "network")
    return SANDBOX_TYPE_NETWORK;
  return SANDBOX_TYPE_UTILITY;
}

}  // namespace content
