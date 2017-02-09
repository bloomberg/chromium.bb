// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/shell_navigation_request.h"

#include "headless/app/headless_shell.h"

namespace headless {

ShellNavigationRequest::ShellNavigationRequest(
    base::WeakPtr<HeadlessShell> headless_shell,
    const page::NavigationRequestedParams& params)
    : headless_shell_(headless_shell),
      navigation_id_(params.GetNavigationId()) {}

ShellNavigationRequest::~ShellNavigationRequest() {}

void ShellNavigationRequest::StartProcessing(base::Closure done_callback) {
  if (!headless_shell_)
    return;

  // Allow the navigation to proceed.
  headless_shell_->devtools_client()
      ->GetPage()
      ->GetExperimental()
      ->ProcessNavigation(
          headless::page::ProcessNavigationParams::Builder()
              .SetNavigationId(navigation_id_)
              .SetResponse(headless::page::NavigationResponse::PROCEED)
              .Build(),
          base::Bind(&ShellNavigationRequest::ProcessNavigationResult,
                     done_callback));
}

// static
void ShellNavigationRequest::ProcessNavigationResult(
    base::Closure done_callback,
    std::unique_ptr<page::ProcessNavigationResult>) {
  done_callback.Run();
}

}  // namespace headless
