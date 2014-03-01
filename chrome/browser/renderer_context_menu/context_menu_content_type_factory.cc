// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/guestview/webview/context_menu_content_type_webview.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_app_mode.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_extension_popup.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_panel.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_platform_app.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"

ContextMenuContentTypeFactory::ContextMenuContentTypeFactory() {
}

ContextMenuContentTypeFactory::~ContextMenuContentTypeFactory() {
}

// static.
ContextMenuContentType* ContextMenuContentTypeFactory::Create(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  if (WebViewGuest::FromWebContents(web_contents))
    return new ContextMenuContentTypeWebView(render_frame_host, params);

  if (chrome::IsRunningInForcedAppMode())
    return new ContextMenuContentTypeAppMode(render_frame_host, params);

  const extensions::ViewType view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::VIEW_TYPE_APP_WINDOW)
    return new ContextMenuContentTypePlatformApp(render_frame_host, params);

  if (view_type == extensions::VIEW_TYPE_EXTENSION_POPUP)
    return new ContextMenuContentTypeExtensionPopup(render_frame_host, params);

  if (view_type == extensions::VIEW_TYPE_PANEL)
    return new ContextMenuContentTypePanel(render_frame_host, params);

  return new ContextMenuContentType(render_frame_host, params, true);
}
