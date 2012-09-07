// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_BROWSER_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_LIB_BROWSER_DELEGATE_IMPL_H_

#include "android_webview/native/aw_browser_dependency_factory.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class URLRequestContextGetter;
}

namespace android_webview {

class AwNetworkDelegate;

class AwBrowserDependencyFactoryImpl : public AwBrowserDependencyFactory {
 public:
  AwBrowserDependencyFactoryImpl();
  virtual ~AwBrowserDependencyFactoryImpl();

  // Sets this class as the singleton instance.
  static void InstallInstance();

  // AwBrowserDependencyFactory
  virtual content::BrowserContext* GetBrowserContext(bool incognito) OVERRIDE;
  virtual content::WebContents* CreateWebContents(bool incognito) OVERRIDE;
  virtual AwContentsContainer* CreateContentsContainer(
      content::WebContents* contents) OVERRIDE;
  virtual content::JavaScriptDialogCreator* GetJavaScriptDialogCreator()
      OVERRIDE;

 private:
  void InitializeNetworkDelegateOnIOThread(
      net::URLRequestContextGetter* normal_context,
      net::URLRequestContextGetter* incognito_context);
  void EnsureNetworkDelegateInitialized();

  // Constructed and assigned on the IO thread.
  scoped_ptr<AwNetworkDelegate> network_delegate_;
  // Set on the UI thread.
  bool initialized_network_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserDependencyFactoryImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_BROWSER_DELEGATE_IMPL_H_

