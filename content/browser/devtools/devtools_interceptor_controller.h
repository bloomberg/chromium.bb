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
class InterceptionHandle;

struct GlobalRequestID;

class DevToolsInterceptorController : public base::SupportsUserData::Data {
 public:
  using GetResponseBodyForInterceptionCallback =
      DevToolsURLRequestInterceptor::GetResponseBodyForInterceptionCallback;
  using RequestInterceptedCallback =
      DevToolsURLRequestInterceptor::RequestInterceptedCallback;
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

  std::unique_ptr<InterceptionHandle> StartInterceptingRequests(
      const FrameTreeNode* target_frame,
      std::vector<Pattern> intercepted_patterns,
      RequestInterceptedCallback callback);

  bool ShouldCancelNavigation(const GlobalRequestID& global_request_id);

  void GetResponseBody(
      std::string request_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback);

  ~DevToolsInterceptorController() override;

 private:
  friend class DevToolsURLRequestInterceptor;

  DevToolsInterceptorController(
      base::WeakPtr<DevToolsURLRequestInterceptor> interceptor,
      std::unique_ptr<DevToolsTargetRegistry> target_registry,
      BrowserContext* browser_context);

  void NavigationStarted(const std::string& interception_id,
                         const GlobalRequestID& request_id);
  void NavigationFinished(const std::string& interception_id);

  base::WeakPtr<DevToolsURLRequestInterceptor> interceptor_;
  std::unique_ptr<DevToolsTargetRegistry> target_registry_;
  base::flat_map<std::string, GlobalRequestID> navigation_requests_;
  base::flat_set<GlobalRequestID> canceled_navigation_requests_;
  base::WeakPtrFactory<DevToolsInterceptorController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsInterceptorController);
};

class InterceptionHandle {
 public:
  ~InterceptionHandle();
  void UpdatePatterns(std::vector<DevToolsURLRequestInterceptor::Pattern>);

 private:
  friend class DevToolsInterceptorController;
  InterceptionHandle(DevToolsTargetRegistry::RegistrationHandle registration,
                     base::WeakPtr<DevToolsURLRequestInterceptor> interceptor,
                     DevToolsURLRequestInterceptor::FilterEntry* entry);

  DevToolsTargetRegistry::RegistrationHandle registration_;
  base::WeakPtr<DevToolsURLRequestInterceptor> interceptor_;
  DevToolsURLRequestInterceptor::FilterEntry* entry_;

  DISALLOW_COPY_AND_ASSIGN(InterceptionHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INTERCEPTOR_CONTROLLER_
