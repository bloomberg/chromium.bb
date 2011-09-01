// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main.h"

#include "base/win/windows_version.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"

namespace content {

void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* info) {
  info->broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!info->broker_services)
    info->target_services = sandbox::SandboxFactory::GetTargetServices();

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // Enforces strong DEP support. Vista uses the NXCOMPAT flag in the exe.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  }
}

}  // namespace content
