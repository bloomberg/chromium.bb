// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_AW_BROWSER_DELEGATE_H_
#define ANDROID_WEBVIEW_AW_BROWSER_DELEGATE_H_

#include "base/basictypes.h"

namespace content {
class BrowserContext;
class JavaScriptDialogCreator;
class WebContents;
}

namespace android_webview {

class AwContentsContainer;

// The concrete class implementing this interface is the responsible for
// creating he browser component objects that the Android WebView depends on.
// The motivation for this abstraction is to keep code under
// android_webview/native from holding direct chrome/ layer dependencies.
// Note this is a distinct role to the Webview embedder proper: this class is
// about 'static' environmental decoupling, allowing dependency injection by the
// top-level lib, whereas runtime behavior is controlled by propagated back to
// the embedding application code via WebContentsDelegate and the like.
class AwBrowserDependencyFactory {
 public:
  virtual ~AwBrowserDependencyFactory();

  // Allows the lib executive to inject the delegate instance. Ownership of
  // |delegate| is NOT transferred, but the object must be long-lived.
  static void SetInstance(AwBrowserDependencyFactory* delegate);

  // Returns the singleton instance. |SetInstance| must have been called.
  static AwBrowserDependencyFactory* GetInstance();

  // Returns the current browser context based on the specified mode.
  virtual content::BrowserContext* GetBrowserContext() = 0;

  // Constructs and returns ownership of a WebContents instance.
  virtual content::WebContents* CreateWebContents() = 0;

 protected:
  AwBrowserDependencyFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(AwBrowserDependencyFactory);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_AW_BROWSER_DELEGATE_H_
