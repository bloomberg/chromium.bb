// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "content/public/browser/web_contents_view_delegate.h"

#include "base/compiler_specific.h"

namespace content {
class WebContents;
}  // namespace content

namespace android_webview {

class AwContents;

class AwWebContentsViewDelegate : public content::WebContentsViewDelegate {
 public:
  static content::WebContentsViewDelegate* Create(
      content::WebContents* web_contents);

  virtual ~AwWebContentsViewDelegate();

  // content::WebContentsViewDelegate implementation.
  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE;
  virtual void ShowContextMenu(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params) OVERRIDE;

 private:
  AwWebContentsViewDelegate(content::WebContents* web_contents);

  // Weak pointer due to ownership graph:
  // WebContents->WebContentsView->this.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AwWebContentsViewDelegate);
};

}  // namespace android_webview


#endif  // ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
