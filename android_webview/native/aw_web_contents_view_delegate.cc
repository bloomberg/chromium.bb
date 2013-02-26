// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_web_contents_view_delegate.h"

#include "android_webview/native/aw_contents.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/context_menu_params.h"

namespace android_webview {

// static
content::WebContentsViewDelegate* AwWebContentsViewDelegate::Create(
    content::WebContents* web_contents) {
  return new AwWebContentsViewDelegate(web_contents);
}

AwWebContentsViewDelegate::AwWebContentsViewDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // Cannot instantiate web_contents_view_delegate_ here because
  // AwContents::SetWebDelegate is not called yet.
}

AwWebContentsViewDelegate::~AwWebContentsViewDelegate() {}

content::WebDragDestDelegate* AwWebContentsViewDelegate::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
  return NULL;
}

void AwWebContentsViewDelegate::ShowContextMenu(
    const content::ContextMenuParams& params,
    content::ContextMenuSourceType type) {
  // TODO(boliu): Large blocks of this function are identical with
  // ChromeWebContentsViewDelegateAndroid::ShowContextMenu. De-dup this if
  // possible.

  // Display paste pop-up only when selection is empty and editable.
  if (params.is_editable && params.selection_text.empty()) {
    content::ContentViewCore* content_view_core =
        web_contents_->GetView()->GetContentNativeView();
    if (content_view_core) {
      content_view_core->ShowPastePopup(params.selection_start.x(),
                                        params.selection_start.y());
      return;
    }
  }

  AwContents* aw_contents = AwContents::FromWebContents(web_contents_);
  if (!aw_contents)
    return;

  // Context menu callback in Chromium is triggered by WebKit on an
  // unhandled GestureLongPress event. Convert the context menu callback
  // back into a long click event for Android WebView since the view
  // system may choose to handle the gesture.
  aw_contents->PerformLongClick();
}

}  // namespace android_webview
