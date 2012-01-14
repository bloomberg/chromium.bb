// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
class HostResolver;
class ProxyConfigService;
class URLRequestContextStorage;
}

namespace content {

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      const FilePath& base_path,
      MessageLoop* io_loop,
      MessageLoop* file_loop);
  virtual ~ShellURLRequestContextGetter();

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual net::CookieStore* DONTUSEME_GetCookieStore() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

  net::HostResolver* host_resolver();

 private:
  FilePath base_path_;
  MessageLoop* io_loop_;
  MessageLoop* file_loop_;

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;

  scoped_refptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequestContextStorage> storage_;

  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
