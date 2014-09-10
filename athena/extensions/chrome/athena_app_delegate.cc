// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_app_delegate.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/env/public/athena_env.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

namespace athena {
namespace {

content::WebContents* OpenURLInActivity(
    content::BrowserContext* context,
    const content::OpenURLParams& params) {
  // Force all links to open in a new activity.
  Activity* activity = ActivityFactory::Get()->CreateWebActivity(
      context, base::string16(), params.url);
  ActivityManager::Get()->AddActivity(activity);
  // TODO(oshima): Get the web cotnents from activity.
  return NULL;
}

}  // namespace

// This is a extra step to open a new Activity when a link is simply clicked
// on an app activity (which usually replaces the content).
class AthenaAppDelegate::NewWindowContentsDelegate
    : public content::WebContentsDelegate {
 public:
  NewWindowContentsDelegate() {}
  virtual ~NewWindowContentsDelegate() {}

  // content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE {
    if (!source)
      return NULL;

    return OpenURLInActivity(source->GetBrowserContext(), params);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowContentsDelegate);
};

AthenaAppDelegate::AthenaAppDelegate()
    : new_window_contents_delegate_(new NewWindowContentsDelegate()) {
}

AthenaAppDelegate::~AthenaAppDelegate() {
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->RemoveTerminatingCallback(terminating_callback_);
}

void AthenaAppDelegate::InitWebContents(content::WebContents* web_contents) {
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

void AthenaAppDelegate::ResizeWebContents(content::WebContents* web_contents,
                                          const gfx::Size& size) {
  ::ResizeWebContents(web_contents, size);
}

content::WebContents* AthenaAppDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLInActivity(context, params);
}

void AthenaAppDelegate::AddNewContents(content::BrowserContext* context,
                                       content::WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture,
                                       bool* was_blocked) {
  new_contents->SetDelegate(new_window_contents_delegate_.get());
}

content::ColorChooser* AthenaAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void AthenaAppDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void AthenaAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

int AthenaAppDelegate::PreferredIconSize() {
  // TODO(oshima): Find out what to use.
  return extension_misc::EXTENSION_ICON_SMALL;
}

gfx::ImageSkia AthenaAppDelegate::GetAppDefaultIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

void AthenaAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(host->GetRoutingID(),
                                                         blocked));
  }
}

bool AthenaAppDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void AthenaAppDelegate::SetTerminatingCallback(const base::Closure& callback) {
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->RemoveTerminatingCallback(terminating_callback_);
  terminating_callback_ = callback;
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->AddTerminatingCallback(terminating_callback_);
}

}  // namespace athena
