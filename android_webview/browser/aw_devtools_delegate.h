// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

namespace content {
class BrowserContext;
class DevToolsHttpHandler;
}

namespace android_webview {

class AwDevToolsDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  AwDevToolsDelegate(content::BrowserContext* browser_context);
  virtual ~AwDevToolsDelegate();

  // Stops http server.
  void Stop();

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual base::FilePath GetDebugFrontendDir() OVERRIDE;
  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE;
  virtual content::RenderViewHost* CreateNewTarget() OVERRIDE;
  virtual TargetType GetTargetType(content::RenderViewHost*) OVERRIDE;

  content::DevToolsHttpHandler* devtools_http_handler() {
    return devtools_http_handler_;
  }

 private:
  content::BrowserContext* browser_context_;
  content::DevToolsHttpHandler* devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(AwDevToolsDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DELEGATE_H_
