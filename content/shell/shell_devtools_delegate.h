// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_DEVTOOLS_DELEGATE_H_
#define CONTENT_SHELL_SHELL_DEVTOOLS_DELEGATE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class DevToolsHttpHandler;

class ShellDevToolsDelegate
    : public content::DevToolsHttpHandlerDelegate,
      public content::WebContentsObserver {
 public:
  ShellDevToolsDelegate(int port, net::URLRequestContextGetter* context_getter);
  virtual ~ShellDevToolsDelegate();

  // Stops http server.
  void Stop();

  // WebContentsObserver overrides.
  virtual void WebContentsDestroyed(WebContents* contents) OVERRIDE;

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual DevToolsHttpHandlerDelegate::InspectableTabs
      GetInspectableTabs() OVERRIDE;
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual std::string GetFrontendResourcesBaseURL() OVERRIDE;

  void AddWebContents(WebContents* web_contents);

 private:
  std::vector<WebContents*> web_contents_list_;
  net::URLRequestContextGetter* context_getter_;
  DevToolsHttpHandler* devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(ShellDevToolsDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_DEVTOOLS_DELEGATE_H_
