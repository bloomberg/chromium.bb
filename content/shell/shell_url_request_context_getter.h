// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

class MessageLoop;

namespace net {
class HostResolver;
class MappedHostResolver;
class NetworkDelegate;
class ProxyConfigService;
class URLRequestContextStorage;
}

namespace content {

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      bool ignore_certificate_errors,
      const base::FilePath& base_path,
      MessageLoop* io_loop,
      MessageLoop* file_loop,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          blob_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          file_system_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          developer_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_devtools_protocol_handler);

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

  net::HostResolver* host_resolver();

 protected:
  virtual ~ShellURLRequestContextGetter();

 private:
  bool ignore_certificate_errors_;
  base::FilePath base_path_;
  MessageLoop* io_loop_;
  MessageLoop* file_loop_;

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::NetworkDelegate> network_delegate_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      blob_protocol_handler_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      file_system_protocol_handler_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      developer_protocol_handler_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      chrome_protocol_handler_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      chrome_devtools_protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
