// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"

#include "chrome/browser/renderer_context_menu/context_menu_content_type.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/guest_view/web_view/context_menu_content_type_web_view.h"
#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_app_mode.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_extension_popup.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_panel.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_platform_app.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

ContextMenuContentTypeFactory::ContextMenuContentTypeFactory() {
}

ContextMenuContentTypeFactory::~ContextMenuContentTypeFactory() {
}

// static.
ContextMenuContentType* ContextMenuContentTypeFactory::Create(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
#if defined(ENABLE_EXTENSIONS)
  if (WebViewGuest::FromWebContents(web_contents))
    return new ContextMenuContentTypeWebView(web_contents, params);

  if (chrome::IsRunningInForcedAppMode())
    return new ContextMenuContentTypeAppMode(web_contents, params);

  const extensions::ViewType view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::VIEW_TYPE_APP_WINDOW)
    return new ContextMenuContentTypePlatformApp(web_contents, params);

  if (view_type == extensions::VIEW_TYPE_EXTENSION_POPUP)
    return new ContextMenuContentTypeExtensionPopup(web_contents, params);

  if (view_type == extensions::VIEW_TYPE_PANEL)
    return new ContextMenuContentTypePanel(web_contents, params);
#endif

  return new ContextMenuContentType(web_contents, params, true);
}
