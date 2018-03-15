// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_network_interceptor.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class FrameTreeNode;

class DevToolsURLLoaderInterceptor {
 public:
  class Impl;

  DevToolsURLLoaderInterceptor(
      FrameTreeNode* local_root,
      DevToolsNetworkInterceptor::RequestInterceptedCallback callback);
  ~DevToolsURLLoaderInterceptor();

  void SetPatterns(std::vector<DevToolsNetworkInterceptor::Pattern> patterns);

  void GetResponseBody(
      const std::string& interception_id,
      std::unique_ptr<
          DevToolsNetworkInterceptor::GetResponseBodyForInterceptionCallback>
          callback);
  void ContinueInterceptedRequest(
      const std::string& interception_id,
      std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications,
      std::unique_ptr<
          DevToolsNetworkInterceptor::ContinueInterceptedRequestCallback>
          callback);

  bool CreateProxyForInterception(
      const base::UnguessableToken frame_token,
      network::mojom::URLLoaderFactoryRequest* request) const;

 private:
  void UpdateSubresourceLoaderFactories();

  FrameTreeNode* const local_root_;
  bool enabled_;
  std::unique_ptr<Impl, base::OnTaskRunnerDeleter> impl_;
  base::WeakPtr<Impl> weak_impl_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
