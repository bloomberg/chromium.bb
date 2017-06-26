// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
#define CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_

namespace sandbox {
class SeatbeltExecClient;
}

namespace content {

// Populates the |client| with the parameters that the sandbox needs to
// resolve information that cannot be known at build time, such as
// the user's home directory.
void SetupRendererSandboxParameters(sandbox::SeatbeltExecClient* client);

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
