// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_PERMISSION_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_PERMISSION_CLIENT_H_

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebPermissionClient.h"

namespace android_webview {

// Android WebView implementation of blink::WebPermissionClient.
class AwPermissionClient : public content::RenderFrameObserver,
                           public blink::WebPermissionClient {
 public:
  explicit AwPermissionClient(content::RenderFrame* render_view);

 private:  
  virtual ~AwPermissionClient();

  // blink::WebPermissionClient implementation.
  virtual bool allowDisplayingInsecureContent(
      bool enabled_per_settings,
      const blink::WebSecurityOrigin& origin,
      const blink::WebURL& url);
  virtual bool allowRunningInsecureContent(
      bool enabled_per_settings,
      const blink::WebSecurityOrigin& origin,
      const blink::WebURL& url);

  DISALLOW_COPY_AND_ASSIGN(AwPermissionClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_PERMISSION_CLIENT_H_
