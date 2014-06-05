// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/context_menu_content_type_web_view.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

ContextMenuContentTypeWebView::ContextMenuContentTypeWebView(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(web_contents, params, true) {
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
    case ITEM_GROUP_PRINT_PREVIEW:
      return false;
    case ITEM_GROUP_CURRENT_EXTENSION:
      // Show contextMenus API items.
      return true;
    case ITEM_GROUP_DEVELOPER:
      // TODO(lazyboy): Enable this for mac too when http://crbug.com/380405 is
      // fixed.
#if !defined(OS_MACOSX)
      {
        // Add dev tools for unpacked extensions.
        const extensions::Extension* embedder_platform_app = GetExtension();
        return !embedder_platform_app ||
            extensions::Manifest::IsUnpackedLocation(
                embedder_platform_app->location()) ||
            CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kDebugPackedApps);
      }
#else
      return ContextMenuContentType::SupportsGroup(group);
#endif
    default:
      return ContextMenuContentType::SupportsGroup(group);
  }
}
