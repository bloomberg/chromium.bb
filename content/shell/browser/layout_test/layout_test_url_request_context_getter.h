// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
class MessageLoop;
}

namespace net {
class HostResolver;
class MappedHostResolver;
class NetworkDelegate;
class NetLog;
class ProxyConfigService;
class URLRequestContextStorage;
}

namespace content {

class LayoutTestURLRequestContextGetter : public ShellURLRequestContextGetter {
 public:
  LayoutTestURLRequestContextGetter(
      bool ignore_certificate_errors,
      const base::FilePath& base_path,
      base::MessageLoop* io_loop,
      base::MessageLoop* file_loop,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors,
      net::NetLog* net_log);

 protected:
  ~LayoutTestURLRequestContextGetter() override;

  // ShellURLRequestContextGetter implementation.
  net::NetworkDelegate* CreateNetworkDelegate() override;
  net::ProxyConfigService* GetProxyConfigService() override;
  net::ProxyService* GetProxyService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutTestURLRequestContextGetter);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_URL_REQUEST_CONTEXT_GETTER_H_
