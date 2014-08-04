// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_web_contents_helper.h"

#include "apps/app_delegate.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/api_permission.h"

namespace apps {

AppWebContentsHelper::AppWebContentsHelper(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    content::WebContents* web_contents,
    AppDelegate* app_delegate)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      web_contents_(web_contents),
      app_delegate_(app_delegate) {
}

// static
bool AppWebContentsHelper::ShouldSuppressGestureEvent(
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming in app windows.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
         event.type == blink::WebGestureEvent::GesturePinchUpdate ||
         event.type == blink::WebGestureEvent::GesturePinchEnd;
}

content::WebContents* AppWebContentsHelper::OpenURLFromTab(
    const content::OpenURLParams& params) const {
  // Don't allow the current tab to be navigated. It would be nice to map all
  // anchor tags (even those without target="_blank") to new tabs, but right
  // now we can't distinguish between those and <meta> refreshes or window.href
  // navigations, which we don't want to allow.
  // TOOD(mihaip): Can we check for user gestures instead?
  WindowOpenDisposition disposition = params.disposition;
  if (disposition == CURRENT_TAB) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't open same-window link to \"%s\"; try target=\"_blank\".",
            params.url.spec().c_str()));
    return NULL;
  }

  // These dispositions aren't really navigations.
  if (disposition == SUPPRESS_OPEN || disposition == SAVE_TO_DISK ||
      disposition == IGNORE_ACTION) {
    return NULL;
  }

  content::WebContents* contents =
      app_delegate_->OpenURLFromTab(browser_context_, web_contents_, params);
  if (!contents) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
  }

  return contents;
}

void AppWebContentsHelper::RequestToLockMouse() const {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  bool has_permission = IsExtensionWithPermissionOrSuggestInConsole(
      extensions::APIPermission::kPointerLock,
      extension,
      web_contents_->GetRenderViewHost());

  web_contents_->GotResponseToLockMouseRequest(has_permission);
}

void AppWebContentsHelper::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) const {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  app_delegate_->RequestMediaAccessPermission(
      web_contents_, request, callback, extension);
}

const extensions::Extension* AppWebContentsHelper::GetExtension() const {
  return extensions::ExtensionRegistry::Get(browser_context_)
      ->enabled_extensions()
      .GetByID(extension_id_);
}

void AppWebContentsHelper::AddMessageToDevToolsConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) const {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AddMessageToConsole(
      rvh->GetRoutingID(), level, message));
}

}  // namespace apps
