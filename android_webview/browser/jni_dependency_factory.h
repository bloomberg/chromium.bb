// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_

#include "base/memory/ref_counted.h"

namespace content {
class ExternalVideoSurfaceContainer;
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

namespace android_webview {

class AwBrowserContext;
class AwQuotaManagerBridge;
class AwWebPreferencesPopulater;

// Used to create instances of objects under native that are used in browser.
class JniDependencyFactory {
 public:
  virtual ~JniDependencyFactory() {}

  virtual scoped_refptr<AwQuotaManagerBridge> CreateAwQuotaManagerBridge(
      AwBrowserContext* browser_context) = 0;
  virtual content::WebContentsViewDelegate* CreateViewDelegate(
      content::WebContents* web_contents) = 0;
  virtual AwWebPreferencesPopulater* CreateWebPreferencesPopulater() = 0;
#if defined(VIDEO_HOLE)
  virtual content::ExternalVideoSurfaceContainer*
      CreateExternalVideoSurfaceContainer(content::WebContents* contents) = 0;
#endif
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JNI_DEPENDENCY_FACTORY_H_
