// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/constants.h"

#if defined(USE_ASH)
#include "ash/shelf/shelf_constants.h"
#endif

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

namespace {

bool disable_external_open_for_testing_ = false;

// Opens a URL with Chromium (not external browser) with the right profile.
content::WebContents* OpenURLFromTabInternal(
    content::BrowserContext* context,
    const content::OpenURLParams& params) {
  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  chrome::NavigateParams new_tab_params(
      static_cast<Browser*>(NULL), params.url, params.transition);
  if (params.disposition == NEW_BACKGROUND_TAB) {
    new_tab_params.disposition = NEW_BACKGROUND_TAB;
  } else {
    new_tab_params.disposition = NEW_FOREGROUND_TAB;
    new_tab_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  }

  new_tab_params.initiating_profile = Profile::FromBrowserContext(context);
  chrome::Navigate(&new_tab_params);

  return new_tab_params.target_contents;
}

// Helper class that opens a URL based on if this browser instance is the
// default system browser. If it is the default, open the URL directly instead
// of asking the system to open it.
class OpenURLFromTabBasedOnBrowserDefault
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  OpenURLFromTabBasedOnBrowserDefault(scoped_ptr<content::WebContents> source,
                                      const content::OpenURLParams& params)
      : source_(source.Pass()), params_(params) {}

  // Opens a URL when called with the result of if this is the default system
  // browser or not.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE {
    Profile* profile =
        Profile::FromBrowserContext(source_->GetBrowserContext());
    DCHECK(profile);
    if (!profile)
      return;
    switch (state) {
      case ShellIntegration::STATE_PROCESSING:
        break;
      case ShellIntegration::STATE_IS_DEFAULT:
        OpenURLFromTabInternal(profile, params_);
        break;
      case ShellIntegration::STATE_NOT_DEFAULT:
      case ShellIntegration::STATE_UNKNOWN:
        platform_util::OpenExternal(profile, params_.url);
        break;
    }
  }

  virtual bool IsOwnedByWorker() OVERRIDE { return true; }

 private:
  scoped_ptr<content::WebContents> source_;
  const content::OpenURLParams params_;
};

}  // namespace

class ChromeAppWindowDelegate::NewWindowContentsDelegate
    : public content::WebContentsDelegate {
 public:
  NewWindowContentsDelegate() {}
  virtual ~NewWindowContentsDelegate() {}

  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowContentsDelegate);
};

content::WebContents*
ChromeAppWindowDelegate::NewWindowContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (source) {
    // This NewWindowContentsDelegate was given ownership of the incoming
    // WebContents by being assigned as its delegate within
    // ChromeAppWindowDelegate::AddNewContents, but this is the first time
    // NewWindowContentsDelegate actually sees the WebContents.
    // Here it is captured for deletion.
    scoped_ptr<content::WebContents> owned_source(source);
    scoped_refptr<ShellIntegration::DefaultWebClientWorker>
        check_if_default_browser_worker =
            new ShellIntegration::DefaultBrowserWorker(
                new OpenURLFromTabBasedOnBrowserDefault(owned_source.Pass(),
                                                        params));
    // Object lifetime notes: The OpenURLFromTabBasedOnBrowserDefault is owned
    // by check_if_default_browser_worker. StartCheckIsDefault() takes lifetime
    // ownership of check_if_default_browser_worker and will clean up after
    // the asynchronous tasks.
    check_if_default_browser_worker->StartCheckIsDefault();
  }
  return NULL;
}

ChromeAppWindowDelegate::ChromeAppWindowDelegate()
    : new_window_contents_delegate_(new NewWindowContentsDelegate()) {}

ChromeAppWindowDelegate::~ChromeAppWindowDelegate() {}

void ChromeAppWindowDelegate::DisableExternalOpenForTesting() {
  disable_external_open_for_testing_ = true;
}

void ChromeAppWindowDelegate::InitWebContents(
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
}

apps::NativeAppWindow* ChromeAppWindowDelegate::CreateNativeAppWindow(
    apps::AppWindow* window,
    const apps::AppWindow::CreateParams& params) {
  return CreateNativeAppWindowImpl(window, params);
}

content::WebContents* ChromeAppWindowDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLFromTabInternal(context, params);
}

void ChromeAppWindowDelegate::AddNewContents(content::BrowserContext* context,
                                             content::WebContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture,
                                             bool* was_blocked) {
  if (!disable_external_open_for_testing_) {
    // We don't really want to open a window for |new_contents|, but we need to
    // capture its intended navigation. Here we give ownership to the
    // NewWindowContentsDelegate, which will dispose of the contents once
    // a navigation is captured.
    new_contents->SetDelegate(new_window_contents_delegate_.get());
    return;
  }
  chrome::ScopedTabbedBrowserDisplayer displayer(
      Profile::FromBrowserContext(context), chrome::GetActiveDesktop());
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  chrome::AddWebContents(displayer.browser(), NULL, new_contents, disposition,
                         initial_pos, user_gesture, was_blocked);
}

content::ColorChooser* ChromeAppWindowDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ChromeAppWindowDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void ChromeAppWindowDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

int ChromeAppWindowDelegate::PreferredIconSize() {
#if defined(USE_ASH)
  return ash::kShelfSize;
#else
  return extension_misc::EXTENSION_ICON_SMALL;
#endif
}

void ChromeAppWindowDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(
        host->GetRoutingID(), blocked));
  }
}

bool ChromeAppWindowDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}
