// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_content_browser_client.h"

#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_context.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"

namespace headless {

HeadlessContentBrowserClient::HeadlessContentBrowserClient(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessContentBrowserClient::~HeadlessContentBrowserClient() {}

content::BrowserMainParts* HeadlessContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams&) {
  scoped_ptr<HeadlessBrowserMainParts> browser_main_parts =
      make_scoped_ptr(new HeadlessBrowserMainParts(browser_));
  browser_->set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts.release();
}

net::URLRequestContextGetter*
HeadlessContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  CHECK(content_browser_context == browser_context());
  scoped_refptr<HeadlessURLRequestContextGetter> url_request_context_getter(
      new HeadlessURLRequestContextGetter(
          false /* ignore_certificate_errors */, browser_context()->GetPath(),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE),
          protocol_handlers, std::move(request_interceptors),
          nullptr /* net_log */, browser_context()->options()));
  browser_context()->SetURLRequestContextGetter(url_request_context_getter);
  return url_request_context_getter.get();
}

HeadlessBrowserContext* HeadlessContentBrowserClient::browser_context() const {
  return browser_->browser_context();
}

}  // namespace headless
