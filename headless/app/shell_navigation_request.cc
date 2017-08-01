// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/shell_navigation_request.h"

#include "headless/app/headless_shell.h"

namespace headless {

ShellNavigationRequest::ShellNavigationRequest(
    base::WeakPtr<HeadlessShell> headless_shell,
    const network::RequestInterceptedParams& params)
    : headless_shell_(headless_shell),
      interception_id_(params.GetInterceptionId()) {}

ShellNavigationRequest::~ShellNavigationRequest() {}

void ShellNavigationRequest::StartProcessing(base::Closure done_callback) {
  if (!headless_shell_)
    return;

  // Allow the navigation to proceed.
  headless_shell_->devtools_client()
      ->GetNetwork()
      ->GetExperimental()
      ->ContinueInterceptedRequest(
          headless::network::ContinueInterceptedRequestParams::Builder()
              .SetInterceptionId(interception_id_)
              .Build(),
          base::Bind(&ShellNavigationRequest::ContinueInterceptedRequestResult,
                     done_callback));
}

// static
void ShellNavigationRequest::ContinueInterceptedRequestResult(
    base::Closure done_callback,
    std::unique_ptr<network::ContinueInterceptedRequestResult>) {
  done_callback.Run();
}

}  // namespace headless
