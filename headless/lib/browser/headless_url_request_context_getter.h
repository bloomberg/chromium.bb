// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "headless/public/headless_browser.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_getter.h"
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
class ProxyService;
class URLRequestContextStorage;
}

namespace headless {

class HeadlessURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  HeadlessURLRequestContextGetter(
      bool ignore_certificate_errors,
      const base::FilePath& base_path,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      net::NetLog* net_log,
      const HeadlessBrowser::Options& options);

  // net::URLRequestContextGetter implementation:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  net::HostResolver* host_resolver() const;

 protected:
  ~HeadlessURLRequestContextGetter() override;

  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate();
  std::unique_ptr<net::ProxyConfigService> GetProxyConfigService();
  std::unique_ptr<net::ProxyService> GetProxyService();

 private:
  bool ignore_certificate_errors_;
  base::FilePath base_path_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  net::NetLog* net_log_;
  HeadlessBrowser::Options options_;

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::NetworkDelegate> network_delegate_;
  std::unique_ptr<net::URLRequestContextStorage> storage_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessURLRequestContextGetter);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_
