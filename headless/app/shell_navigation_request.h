// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_HTTP_URL_FETCHER_H_
#define HEADLESS_PUBLIC_UTIL_HTTP_URL_FETCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/util/navigation_request.h"

namespace headless {
class HeadlessShell;

// Used in deterministic mode to make sure navigations and resource requests
// complete in the order requested.
class ShellNavigationRequest : public NavigationRequest {
 public:
  ShellNavigationRequest(base::WeakPtr<HeadlessShell> headless_shell,
                         const network::RequestInterceptedParams& params);

  ~ShellNavigationRequest() override;

  void StartProcessing(base::Closure done_callback) override;

 private:
  // Note the navigation likely isn't done when this is called, however we
  // expect it will have been committed and the initial resource load requested.
  static void ContinueInterceptedRequestResult(
      base::Closure done_callback,
      std::unique_ptr<network::ContinueInterceptedRequestResult>);

  base::WeakPtr<HeadlessShell> headless_shell_;
  std::string interception_id_;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_HTTP_URL_FETCHER_H_
