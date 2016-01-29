// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/blimp_browser_main_parts.h"
#include "blimp/engine/common/blimp_browser_context.h"

namespace blimp {
namespace engine {

BlimpContentBrowserClient::BlimpContentBrowserClient() {}

BlimpContentBrowserClient::~BlimpContentBrowserClient() {}

content::BrowserMainParts* BlimpContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  blimp_browser_main_parts_ = new BlimpBrowserMainParts(parameters);
  // BrowserMainLoop takes ownership of the returned BrowserMainParts.
  return blimp_browser_main_parts_;
}

net::URLRequestContextGetter* BlimpContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  BlimpBrowserContext* blimp_context =
      static_cast<BlimpBrowserContext*>(content_browser_context);
  return blimp_context->CreateRequestContext(protocol_handlers,
                                             std::move(request_interceptors))
      .get();
}

BlimpBrowserContext* BlimpContentBrowserClient::GetBrowserContext() {
  return blimp_browser_main_parts_->GetBrowserContext();
}

}  // namespace engine
}  // namespace blimp
