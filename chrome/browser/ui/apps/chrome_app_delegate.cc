// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_delegate.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/data_use_measurement/data_use_web_contents_observer.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/constants.h"
#include "printing/features/features.h"

#if defined(USE_ASH)
#include "ash/common/shelf/shelf_constants.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

namespace {

// Time to wait for an app window to show before allowing Chrome to quit.
int kAppWindowFirstShowTimeoutSeconds = 10;

bool disable_external_open_for_testing_ = false;

// Opens a URL with Chromium (not external browser) with the right profile.
content::WebContents* OpenURLFromTabInternal(
    content::BrowserContext* context,
    const content::OpenURLParams& params) {
  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  chrome::NavigateParams new_tab_params(
      static_cast<Browser*>(NULL), params.url, params.transition);
  if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    new_tab_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  } else {
    new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    new_tab_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  }

  new_tab_params.initiating_profile = Profile::FromBrowserContext(context);
  chrome::Navigate(&new_tab_params);

  return new_tab_params.target_contents;
}

void OnCheckIsDefaultBrowserFinished(
    std::unique_ptr<content::WebContents> source,
    const content::OpenURLParams& params,
    shell_integration::DefaultWebClientState state) {
  // Open a URL based on if this browser instance is the default system browser.
  // If it is the default, open the URL directly instead of asking the system to
  // open it.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  DCHECK(profile);
  if (!profile)
    return;
  switch (state) {
    case shell_integration::IS_DEFAULT:
      OpenURLFromTabInternal(profile, params);
      break;
    case shell_integration::NOT_DEFAULT:
    case shell_integration::UNKNOWN_DEFAULT:
      platform_util::OpenExternal(profile, params.url);
      break;
    case shell_integration::NUM_DEFAULT_STATES:
      NOTREACHED();
      break;
  }
}

}  // namespace

// static
void ChromeAppDelegate::RelinquishKeepAliveAfterTimeout(
    const base::WeakPtr<ChromeAppDelegate>& chrome_app_delegate) {
  // Resetting the ScopedKeepAlive may cause nested destruction of the
  // ChromeAppDelegate which also resets the ScopedKeepAlive. To avoid this,
  // move the ScopedKeepAlive out to here and let it fall out of scope.
  if (chrome_app_delegate.get() && chrome_app_delegate->is_hidden_)
    std::unique_ptr<ScopedKeepAlive>(
        std::move(chrome_app_delegate->keep_alive_));
}

class ChromeAppDelegate::NewWindowContentsDelegate
    : public content::WebContentsDelegate {
 public:
  NewWindowContentsDelegate() {}
  ~NewWindowContentsDelegate() override {}

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowContentsDelegate);
};

content::WebContents*
ChromeAppDelegate::NewWindowContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (source) {
    // This NewWindowContentsDelegate was given ownership of the incoming
    // WebContents by being assigned as its delegate within
    // ChromeAppDelegate::AddNewContents(), but this is the first time
    // NewWindowContentsDelegate actually sees the WebContents. Here ownership
    // is captured and passed to OnCheckIsDefaultBrowserFinished(), which
    // destroys it after the default browser worker completes.
    std::unique_ptr<content::WebContents> source_ptr(source);
    // Object lifetime notes: StartCheckIsDefault() takes lifetime ownership of
    // check_if_default_browser_worker and will clean up after the asynchronous
    // tasks.
    scoped_refptr<shell_integration::DefaultBrowserWorker>
        check_if_default_browser_worker =
            new shell_integration::DefaultBrowserWorker(
                base::Bind(&OnCheckIsDefaultBrowserFinished,
                           base::Passed(&source_ptr), params));
    check_if_default_browser_worker->StartCheckIsDefault();
  }
  return NULL;
}

ChromeAppDelegate::ChromeAppDelegate(bool keep_alive)
    : has_been_shown_(false),
      is_hidden_(true),
      new_window_contents_delegate_(new NewWindowContentsDelegate()),
      weak_factory_(this) {
  if (keep_alive) {
    keep_alive_.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                          KeepAliveRestartOption::DISABLED));
  }
  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

ChromeAppDelegate::~ChromeAppDelegate() {
  // Unregister now to prevent getting notified if |keep_alive_| is the last.
  terminating_callback_.Reset();
}

void ChromeAppDelegate::DisableExternalOpenForTesting() {
  disable_external_open_for_testing_ = true;
}

void ChromeAppDelegate::InitWebContents(content::WebContents* web_contents) {
  data_use_measurement::DataUseWebContentsObserver::CreateForWebContents(
      web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);

  zoom::ZoomController::CreateForWebContents(web_contents);
}

void ChromeAppDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (!chrome::IsRunningInForcedAppMode()) {
    // Due to a bug in the way apps reacted to default zoom changes, some apps
    // can incorrectly have host level zoom settings. These aren't wanted as
    // apps cannot be zoomed, so are removed. This should be removed if apps
    // can be made to zoom again.
    // See http://crbug.com/446759 for more details.
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(render_view_host);
    DCHECK(web_contents);
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetForWebContents(web_contents);
    DCHECK(zoom_map);
    zoom_map->SetZoomLevelForHost(web_contents->GetURL().host(), 0);
  }
}

void ChromeAppDelegate::ResizeWebContents(content::WebContents* web_contents,
                                          const gfx::Size& size) {
  ::ResizeWebContents(web_contents, gfx::Rect(size));
}

content::WebContents* ChromeAppDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLFromTabInternal(context, params);
}

void ChromeAppDelegate::AddNewContents(content::BrowserContext* context,
                                       content::WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_rect,
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
      Profile::FromBrowserContext(context));
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition = disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB
                    ? disposition
                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::AddWebContents(displayer.browser(),
                         NULL,
                         new_contents,
                         disposition,
                         initial_rect,
                         user_gesture,
                         was_blocked);
}

content::ColorChooser* ChromeAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ChromeAppDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, params);
}

void ChromeAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

bool ChromeAppDelegate::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(
          web_contents, security_origin, type, extension);
}

int ChromeAppDelegate::PreferredIconSize() {
#if defined(USE_ASH)
  return ash::GetShelfConstant(ash::SHELF_SIZE);
#else
  return extension_misc::EXTENSION_ICON_SMALL;
#endif
}

void ChromeAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  if (!blocked)
    web_contents->Focus();
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(host->GetRoutingID(),
                                                         blocked));
  }
}

bool ChromeAppDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void ChromeAppDelegate::SetTerminatingCallback(const base::Closure& callback) {
  terminating_callback_ = callback;
}

void ChromeAppDelegate::OnHide() {
  is_hidden_ = true;
  if (has_been_shown_) {
    keep_alive_.reset();
    return;
  }

  // Hold on to the keep alive for some time to give the app a chance to show
  // the window.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeAppDelegate::RelinquishKeepAliveAfterTimeout,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kAppWindowFirstShowTimeoutSeconds));
}

void ChromeAppDelegate::OnShow() {
  has_been_shown_ = true;
  is_hidden_ = false;
  keep_alive_.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                        KeepAliveRestartOption::DISABLED));
}

void ChromeAppDelegate::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  if (!terminating_callback_.is_null())
    terminating_callback_.Run();
}
