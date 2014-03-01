// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/context_menu_content_type_webview.h"

ContextMenuContentTypeWebView::ContextMenuContentTypeWebView(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(render_frame_host, params, true) {
}

ContextMenuContentTypeWebView::~ContextMenuContentTypeWebView() {
}

bool ContextMenuContentTypeWebView::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_PAGE:
    case ITEM_GROUP_FRAME:
    case ITEM_GROUP_LINK:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_PRINT:
    case ITEM_GROUP_ALL_EXTENSION:
    case ITEM_GROUP_CURRENT_EXTENSION:
    case ITEM_GROUP_PRINT_PREVIEW:
      return false;
    default:
      return ContextMenuContentType::SupportsGroup(group);
  }
}
