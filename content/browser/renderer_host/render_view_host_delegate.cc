// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

#include "base/memory/singleton.h"
#include "chrome/common/render_messages.h"
#include "content/common/renderer_preferences.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webpreferences.h"

RenderViewHostDelegate::View* RenderViewHostDelegate::GetViewDelegate() {
  return NULL;
}

RenderViewHostDelegate::RendererManagement*
RenderViewHostDelegate::GetRendererManagementDelegate() {
  return NULL;
}

RenderViewHostDelegate::BookmarkDrag*
RenderViewHostDelegate::GetBookmarkDragDelegate() {
  return NULL;
}

bool RenderViewHostDelegate::OnMessageReceived(const IPC::Message& message) {
  return false;
}

const GURL& RenderViewHostDelegate::GetURL() const {
  return GURL::EmptyGURL();
}

TabContents* RenderViewHostDelegate::GetAsTabContents() {
  return NULL;
}

BackgroundContents* RenderViewHostDelegate::GetAsBackgroundContents() {
  return NULL;
}

GURL RenderViewHostDelegate::GetAlternateErrorPageURL() const {
  return GURL();
}

WebPreferences RenderViewHostDelegate::GetWebkitPrefs() {
  return WebPreferences();
}

bool RenderViewHostDelegate::IsExternalTabContainer() const {
  return false;
}

bool RenderViewHostDelegate::RequestDesktopNotificationPermission(
    const GURL& source_origin, int callback_context) {
  return false;
}
