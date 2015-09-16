// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_url_request_context_getter.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "net/proxy/proxy_service.h"

namespace content {

LayoutTestURLRequestContextGetter::LayoutTestURLRequestContextGetter(
    bool ignore_certificate_errors,
    const base::FilePath& base_path,
    base::MessageLoop* io_loop,
    base::MessageLoop* file_loop,
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors,
    net::NetLog* net_log)
    : ShellURLRequestContextGetter(ignore_certificate_errors,
                                   base_path,
                                   io_loop,
                                   file_loop,
                                   protocol_handlers,
                                   request_interceptors.Pass(),
                                   net_log) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

LayoutTestURLRequestContextGetter::~LayoutTestURLRequestContextGetter() {
}

scoped_ptr<net::NetworkDelegate>
LayoutTestURLRequestContextGetter::CreateNetworkDelegate() {
  ShellNetworkDelegate::SetAcceptAllCookies(false);
  return make_scoped_ptr(new ShellNetworkDelegate);
}

scoped_ptr<net::ProxyConfigService>
LayoutTestURLRequestContextGetter::GetProxyConfigService() {
  return nullptr;
}

scoped_ptr<net::ProxyService>
LayoutTestURLRequestContextGetter::GetProxyService() {
  return net::ProxyService::CreateDirect();
}

}  // namespace content
