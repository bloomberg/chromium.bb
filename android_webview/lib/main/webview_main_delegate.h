// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_MAIN_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_MAIN_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserMainRunner;
}

namespace android_webview {

// Android WebView implementation of ContentMainDelegate.
class WebViewMainDelegate : public content::ContentMainDelegate {
 public:
  WebViewMainDelegate();
  virtual ~WebViewMainDelegate();

 private:
  // content::ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual void SandboxInitialized(const std::string& process_type) OVERRIDE;
  virtual int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) OVERRIDE;
  virtual void ProcessExiting(const std::string& process_type) OVERRIDE;
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;
  virtual content::ContentRendererClient*
      CreateContentRendererClient() OVERRIDE;

  scoped_ptr<content::BrowserMainRunner> browser_runner_;
  chrome::ChromeContentClient chrome_content_client_;

  DISALLOW_COPY_AND_ASSIGN(WebViewMainDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_MAIN_DELEGATE_H_
