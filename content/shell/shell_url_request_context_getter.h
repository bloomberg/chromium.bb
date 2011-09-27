// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

class MessageLoop;

namespace net {
class CertVerifier;
class DnsRRResolver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpTransactionFactory;
class ProxyService;
class OriginBoundCertService;
class URLRequestJobFactory;
class URLSecurityManager;
}

namespace content {

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      const FilePath& base_path_,
      MessageLoop* io_loop,
      MessageLoop* file_loop);
  virtual ~ShellURLRequestContextGetter();

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual net::CookieStore* DONTUSEME_GetCookieStore() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

  net::HostResolver* host_resolver() { return host_resolver_.get(); }

 private:
  FilePath base_path_;
  MessageLoop* io_loop_;
  MessageLoop* file_loop_;

  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_refptr<net::URLRequestContext> url_request_context_;

  scoped_ptr<net::HttpTransactionFactory> main_http_factory_;
  scoped_ptr<net::HostResolver> host_resolver_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::OriginBoundCertService> origin_bound_cert_service_;
  scoped_ptr<net::DnsRRResolver> dnsrr_resolver_;
  scoped_ptr<net::ProxyService> proxy_service_;
  scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;

  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
