// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_chrome_app_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

namespace athena {

AthenaChromeAppDelegate::AthenaChromeAppDelegate() {
}

AthenaChromeAppDelegate::~AthenaChromeAppDelegate() {
}

void AthenaChromeAppDelegate::InitWebContents(
    content::WebContents* web_contents) {
  FaviconTabHelper::CreateForWebContents(web_contents);

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

content::ColorChooser* AthenaChromeAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void AthenaChromeAppDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void AthenaChromeAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

bool AthenaChromeAppDelegate::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(
          web_contents, security_origin, type, extension);
}

void AthenaChromeAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(host->GetRoutingID(),
                                                         blocked));
  }
}

}  // namespace athena
