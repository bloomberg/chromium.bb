// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INTERCEPTOR_CONTROLLER_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INTERCEPTOR_CONTROLLER_

#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_url_request_interceptor.h"

namespace content {

class BrowserContext;
class FrameTreeNode;

class DevToolsInterceptorController : public base::SupportsUserData::Data {
 public:
  using ContinueInterceptedRequestCallback =
      DevToolsURLRequestInterceptor::ContinueInterceptedRequestCallback;
  using Modifications = DevToolsURLRequestInterceptor::Modifications;
  using Pattern = DevToolsURLRequestInterceptor::Pattern;

  static DevToolsInterceptorController* FromBrowserContext(
      BrowserContext* context);

  void ContinueInterceptedRequest(
      std::string interception_id,
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);

  void StartInterceptingRequests(
      const FrameTreeNode* target_frame,
      base::WeakPtr<protocol::NetworkHandler> network_handler,
      std::vector<Pattern> intercepted_patterns);

  void StopInterceptingRequests(const FrameTreeNode* target_frame);

  ~DevToolsInterceptorController() override;

 private:
  friend class DevToolsURLRequestInterceptor;
  static void SetUserData(
      BrowserContext* browser_context,
      base::WeakPtr<DevToolsURLRequestInterceptor>,
      std::unique_ptr<DevToolsTargetRegistry> target_registry);

  DevToolsInterceptorController(
      base::WeakPtr<DevToolsURLRequestInterceptor> interceptor,
      std::unique_ptr<DevToolsTargetRegistry> target_registry);

  base::WeakPtr<DevToolsURLRequestInterceptor> interceptor_;
  std::unique_ptr<DevToolsTargetRegistry> target_registry_;
  base::flat_map<base::UnguessableToken,
                 DevToolsTargetRegistry::RegistrationHandle>
      target_handles_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsInterceptorController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INTERCEPTOR_CONTROLLER_
