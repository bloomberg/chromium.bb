// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_

namespace content {
class GeolocationPermissionContext;
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

namespace android_webview {

class AwBrowserContext;
class AwQuotaManagerBridge;

// Used to create instances of objects under native that are used in browser.
class JniDependencyFactory {
 public:
  virtual ~JniDependencyFactory() {}

  virtual AwQuotaManagerBridge* CreateAwQuotaManagerBridge(
      AwBrowserContext* browser_context) = 0;
  virtual content::GeolocationPermissionContext* CreateGeolocationPermission(
      AwBrowserContext* browser_context) = 0;
  virtual content::WebContentsViewDelegate* CreateViewDelegate(
      content::WebContents* web_contents) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_
