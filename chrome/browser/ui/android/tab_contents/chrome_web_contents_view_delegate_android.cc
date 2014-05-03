// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_contents/chrome_web_contents_view_delegate_android.h"

#include "base/logging.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"

ChromeWebContentsViewDelegateAndroid::ChromeWebContentsViewDelegateAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

ChromeWebContentsViewDelegateAndroid::~ChromeWebContentsViewDelegateAndroid() {
}

content::WebDragDestDelegate*
ChromeWebContentsViewDelegateAndroid::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
  return NULL;
}

void ChromeWebContentsViewDelegateAndroid::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Display paste pop-up only when selection is empty and editable.
  if (params.is_editable && params.selection_text.empty()) {
    content::ContentViewCore* content_view_core =
        content::ContentViewCore::FromWebContents(web_contents_);
    if (content_view_core) {
      content_view_core->ShowPastePopup(params.selection_start.x(),
                                        params.selection_start.y());
      return;
    }
  }

  // TODO(dtrainor, kouhei): Give WebView a Populator/delegate so it can use the
  // same context menu code.
  ContextMenuHelper* helper = ContextMenuHelper::FromWebContents(web_contents_);
  if (helper)
    helper->ShowContextMenu(params);
}

namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateAndroid(web_contents);
}

}  // namespace chrome
