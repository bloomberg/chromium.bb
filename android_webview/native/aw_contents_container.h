// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CONTAINER_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CONTAINER_H_

namespace content {
class WebContents;
}

namespace android_webview {

// Abstraction that contains and owns the components that we assemble to
// make a working WebView instance. This holds a role analogous to the
// TabContents class in the chrome module, but at a higher level of abstraction.
class AwContentsContainer {
 public:
  // Destroying the container will tear down the sub-components of this content.
  virtual ~AwContentsContainer() {}

  // Ownership remains with this container.
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  AwContentsContainer() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CONTAINER_H_
