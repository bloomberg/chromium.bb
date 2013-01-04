// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cc/picture_pile_impl.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPermissionClient.h"

namespace WebKit {

class WebNode;
class WebURL;

}  // namespace WebKit

namespace android_webview {

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderViewExt : public content::RenderViewObserver,
                        public WebKit::WebPermissionClient {
 public:
  static void RenderViewCreated(content::RenderView* render_view);

 private:
  AwRenderViewExt(content::RenderView* render_view);
  virtual ~AwRenderViewExt();

  // RenderView::Observer:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;

  void OnDocumentHasImagesRequest(int id);

  void OnDoHitTest(int view_x, int view_y);

  void OnEnableCapturePictureCallback(bool enable);

  void OnPictureUpdate(scoped_refptr<cc::PicturePileImpl> picture);

  // WebKit::WebPermissionClient implementation.
  virtual bool allowImage(WebKit::WebFrame* frame,
                          bool enabledPerSettings,
                          const WebKit::WebURL& imageURL) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AwRenderViewExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
