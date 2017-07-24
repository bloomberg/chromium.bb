// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define CONTENT_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class CertVerifier;
class HostResolver;
class NetworkDelegate;
class NetLog;
class ProxyConfigService;
class ProxyService;
class URLRequestContextStorage;
}

namespace content {

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      bool ignore_certificate_errors,
      bool off_the_record,
      const base::FilePath& base_path,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors,
      net::NetLog* net_log);

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  net::HostResolver* host_resolver();

 protected:
  ~ShellURLRequestContextGetter() override;

  // Used by subclasses to create their own implementation of NetworkDelegate
  // and net::ProxyService.
  virtual std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate();
  virtual std::unique_ptr<net::CertVerifier> GetCertVerifier();
  virtual std::unique_ptr<net::ProxyConfigService> GetProxyConfigService();
  virtual std::unique_ptr<net::ProxyService> GetProxyService();

 private:
  bool ignore_certificate_errors_;
  bool off_the_record_;
  base::FilePath base_path_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  net::NetLog* net_log_;

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::NetworkDelegate> network_delegate_;
  std::unique_ptr<net::URLRequestContextStorage> storage_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ProtocolHandlerMap protocol_handlers_;
  URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
