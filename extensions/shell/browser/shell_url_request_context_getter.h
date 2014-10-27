// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/files/file_path.h"
#include "content/shell/browser/shell_url_request_context_getter.h"

namespace base {
class MessageLoop;
}

namespace content {
class BrowserContext;
}

namespace net {
class NetworkDelegate;
class NetLog;
}

namespace extensions {

class InfoMap;

class ShellURLRequestContextGetter :
  public content::ShellURLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      content::BrowserContext* browser_context,
      bool ignore_certificate_errors,
      const base::FilePath& base_path,
      base::MessageLoop* io_loop,
      base::MessageLoop* file_loop,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      net::NetLog* net_log,
      InfoMap* extension_info_map);

  // content::ShellURLRequestContextGetter implementation.
  net::NetworkDelegate* CreateNetworkDelegate() override;

protected:
 ~ShellURLRequestContextGetter() override;

private:
  content::BrowserContext* browser_context_;
  InfoMap* extension_info_map_;

private:
  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
