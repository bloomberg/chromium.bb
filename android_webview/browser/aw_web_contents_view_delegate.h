// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "content/public/browser/web_contents_view_delegate.h"

#include "base/macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace android_webview {

class AwWebContentsViewDelegate : public content::WebContentsViewDelegate {
 public:
  static content::WebContentsViewDelegate* Create(
      content::WebContents* web_contents);

  ~AwWebContentsViewDelegate() override;

  // content::WebContentsViewDelegate implementation.
  content::WebDragDestDelegate* GetDragDestDelegate() override;

 private:
  AwWebContentsViewDelegate(content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(AwWebContentsViewDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
