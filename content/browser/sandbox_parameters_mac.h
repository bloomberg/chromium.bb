// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
#define CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_

#include "content/common/content_export.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
class CommandLine;
}

namespace sandbox {
class SeatbeltExecClient;
}

namespace content {

// This populates the sandbox parameters in the client for the given
// |sandbox_type|. Some parameters may be extracted from the |command_line|.
CONTENT_EXPORT void SetupSandboxParameters(
    service_manager::SandboxType sandbox_type,
    const base::CommandLine& command_line,
    sandbox::SeatbeltExecClient* client);

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
