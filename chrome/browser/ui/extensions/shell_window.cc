// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/shell_window.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/app_window.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/request_media_access_permission_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/renderer_preferences.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkRegion.h"

#if defined(USE_ASH)
#include "ash/launcher/launcher_types.h"
#endif

namespace app_window = extensions::api::app_window;

using content::BrowserThread;
using content::ConsoleMessageLevel;
using content::RenderViewHost;
using content::ResourceDispatcherHost;
using content::SiteInstance;
using content::WebContents;
using extensions::APIPermission;
using extensions::RequestMediaAccessPermissionHelper;

namespace {
const int kDefaultWidth = 512;
const int kDefaultHeight = 384;

// The preferred icon size for displaying the app icon.
#if defined(USE_ASH)
const int kPreferredIconSize = ash::kLauncherPreferredSize;
#else
const int kPreferredIconSize = extension_misc::EXTENSION_ICON_SMALL;
#endif

void SuspendRenderViewHost(RenderViewHost* rvh) {
  DCHECK(rvh);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourceDispatcherHost::BlockRequestsForRoute,
          base::Unretained(ResourceDispatcherHost::Get()),
          rvh->GetProcess()->GetID(), rvh->GetRoutingID()));
}

}  // namespace

ShellWindow::CreateParams::CreateParams()
  : window_type(ShellWindow::WINDOW_TYPE_DEFAULT),
    frame(ShellWindow::FRAME_CHROME),
    transparent_background(false),
    bounds(INT_MIN, INT_MIN, 0, 0),
    creator_process_id(0), hidden(false) {
}

ShellWindow::CreateParams::~CreateParams() {
}

ShellWindow* ShellWindow::Create(Profile* profile,
                                 const extensions::Extension* extension,
                                 const GURL& url,
                                 const CreateParams& params) {
  // This object will delete itself when the window is closed.
  ShellWindow* window = new ShellWindow(profile, extension);
  window->Init(url, params);
  extensions::ShellWindowRegistry::Get(profile)->AddShellWindow(window);
  return window;
}

ShellWindow::ShellWindow(Profile* profile,
                         const extensions::Extension* extension)
    : profile_(profile),
      extension_(extension),
      web_contents_(NULL),
      window_type_(WINDOW_TYPE_DEFAULT),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile, this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void ShellWindow::Init(const GURL& url,
                       const ShellWindow::CreateParams& params) {
  window_type_ = params.window_type;

  web_contents_.reset(WebContents::Create(WebContents::CreateParams(
      profile(), SiteInstance::CreateForURL(profile(), url))));
  WebContentsModalDialogManager::CreateForWebContents(web_contents_.get());
  FaviconTabHelper::CreateForWebContents(web_contents_.get());
  WebIntentPickerController::CreateForWebContents(web_contents_.get());

  content::WebContentsObserver::Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  chrome::SetViewType(web_contents_.get(), chrome::VIEW_TYPE_APP_SHELL);
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  gfx::Rect bounds = params.bounds;

  if (bounds.width() == 0)
    bounds.set_width(kDefaultWidth);
  if (bounds.height() == 0)
    bounds.set_height(kDefaultHeight);

  // If left and top are left undefined, the native shell window will center
  // the window on the main screen in a platform-defined manner.

  if (!params.window_key.empty()) {
    window_key_ = params.window_key;

    extensions::ShellWindowGeometryCache* cache =
        extensions::ExtensionSystem::Get(profile())->
          shell_window_geometry_cache();
    gfx::Rect cached_bounds;
    if (cache->GetGeometry(extension()->id(), params.window_key,
                           &cached_bounds))
      bounds = cached_bounds;
  }

  ShellWindow::CreateParams new_params = params;

  gfx::Size& minimum_size = new_params.minimum_size;
  gfx::Size& maximum_size = new_params.maximum_size;

  // In the case that minimum size > maximum size, we consider the minimum
  // size to be more important.
  if (maximum_size.width() && maximum_size.width() < minimum_size.width())
    maximum_size.set_width(minimum_size.width());
  if (maximum_size.height() && maximum_size.height() < minimum_size.height())
    maximum_size.set_height(minimum_size.height());

  if (maximum_size.width() && bounds.width() > maximum_size.width())
    bounds.set_width(maximum_size.width());
  if (bounds.width() != INT_MIN && bounds.width() < minimum_size.width())
    bounds.set_width(minimum_size.width());

  if (maximum_size.height() && bounds.height() > maximum_size.height())
    bounds.set_height(maximum_size.height());
  if (bounds.height() != INT_MIN && bounds.height() < minimum_size.height())
    bounds.set_height(minimum_size.height());

  new_params.bounds = bounds;

  native_app_window_.reset(NativeAppWindow::Create(this, new_params));
  OnNativeWindowChanged();

  if (!params.hidden) {
    if (params.window_type == WINDOW_TYPE_PANEL)
      GetBaseWindow()->ShowInactive();  // Panels are not activated by default.
    else
      GetBaseWindow()->Show();
  }

  // If the new view is in the same process as the creator, block the created
  // RVH from loading anything until the background page has had a chance to do
  // any initialization it wants. If it's a different process, the new RVH
  // shouldn't communicate with the background page anyway (e.g. sandboxed).
  if (web_contents_->GetRenderViewHost()->GetProcess()->GetID() ==
      params.creator_process_id) {
    SuspendRenderViewHost(web_contents_->GetRenderViewHost());
  } else {
    VLOG(1) << "ShellWindow created in new process ("
            << web_contents_->GetRenderViewHost()->GetProcess()->GetID()
            << ") != creator (" << params.creator_process_id
            << "). Routing disabled.";
  }

  // TODO(jeremya): there's a bug where navigating a web contents to an
  // extension URL causes it to create a new RVH and discard the old (perfectly
  // usable) one. To work around this, we watch for a RVH_CHANGED message from
  // the web contents (which will be sent during LoadURL) and suspend resource
  // requests on the new RVH to ensure that we block the new RVH from loading
  // anything. It should be okay to remove the NOTIFICATION_RVH_CHANGED
  // registration once http://crbug.com/123007 is fixed.
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &web_contents_->GetController()));
  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  registrar_.RemoveAll();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // Close when the browser is exiting.
  // TODO(mihaip): we probably don't want this in the long run (when platform
  // apps are no longer tied to the browser process).
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Prevent the browser process from shutting down while this window is open.
  browser::StartKeepAlive();

  UpdateExtensionAppIcon();
}

ShellWindow::~ShellWindow() {
  // Unregister now to prevent getting NOTIFICATION_APP_TERMINATING if we're the
  // last window open.
  registrar_.RemoveAll();

  // Remove shutdown prevention.
  browser::EndKeepAlive();
}

void ShellWindow::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  // Get the preferred default devices for the request.
  content::MediaStreamDevices devices;
  media::GetDefaultDevicesForProfile(
      profile_,
      content::IsAudioMediaType(request.audio_type),
      content::IsVideoMediaType(request.video_type),
      &devices);

  RequestMediaAccessPermissionHelper::AuthorizeRequest(
      devices, request, callback, extension(), true);
}

WebContents* ShellWindow::OpenURLFromTab(WebContents* source,
                                         const content::OpenURLParams& params) {
  DCHECK(source == web_contents_);

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

  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  chrome::NavigateParams new_tab_params(
      static_cast<Browser*>(NULL), params.url, params.transition);
  new_tab_params.disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  new_tab_params.initiating_profile = profile_;
  chrome::Navigate(&new_tab_params);

  if (!new_tab_params.target_contents) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
  }

  return new_tab_params.target_contents;
}

void ShellWindow::AddNewContents(WebContents* source,
                                 WebContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 bool* was_blocked) {
  DCHECK(source == web_contents_);
  DCHECK(Profile::FromBrowserContext(new_contents->GetBrowserContext()) ==
      profile_);
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(profile_, chrome::GetActiveDesktop());
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  chrome::AddWebContents(browser, NULL, new_contents, disposition, initial_pos,
                         user_gesture, was_blocked);
}

void ShellWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(source, web_contents_);
  native_app_window_->HandleKeyboardEvent(event);
}

void ShellWindow::RequestToLockMouse(WebContents* web_contents,
                                     bool user_gesture,
                                     bool last_unlocked_by_target) {
  bool has_permission = IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kPointerLock,
      extension_,
      web_contents->GetRenderViewHost());

  web_contents->GotResponseToLockMouseRequest(has_permission);
}

void ShellWindow::OnNativeClose() {
  extensions::ShellWindowRegistry::Get(profile_)->RemoveShellWindow(this);
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AppWindowClosed(rvh->GetRoutingID()));
  delete this;
}

void ShellWindow::OnNativeWindowChanged() {
  SaveWindowPosition();
  if (!native_app_window_ || !web_contents_)
    return;
  ListValue args;
  DictionaryValue* dictionary = new DictionaryValue();
  args.Append(dictionary);

  gfx::Rect bounds = native_app_window_->GetBounds();
  bounds.Inset(native_app_window_->GetFrameInsets());
  app_window::Bounds update;
  update.left.reset(new int(bounds.x()));
  update.top.reset(new int(bounds.y()));
  update.width.reset(new int(bounds.width()));
  update.height.reset(new int(bounds.height()));
  dictionary->Set("bounds", update.ToValue().release());
  dictionary->SetBoolean("minimized", native_app_window_->IsMinimized());
  dictionary->SetBoolean("maximized", native_app_window_->IsMaximized());

  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_MessageInvoke(rvh->GetRoutingID(),
                                           extension_->id(),
                                           "updateAppWindowProperties",
                                           args,
                                           GURL(),
                                           false));
}

gfx::Image* ShellWindow::GetAppListIcon() {
  // TODO(skuhne): We might want to use LoadImages in UpdateExtensionAppIcon
  // instead to let the extension give us pre-defined icons in the launcher
  // and the launcher list sizes. Since there is no mock yet, doing this now
  // seems a bit premature and we scale for the time being.
  if (app_icon_.IsEmpty())
    return new gfx::Image();

  SkBitmap bmp = skia::ImageOperations::Resize(
        *app_icon_.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
        extension_misc::EXTENSION_ICON_SMALLISH,
        extension_misc::EXTENSION_ICON_SMALLISH);
  return new gfx::Image(bmp);
}

NativeAppWindow* ShellWindow::GetBaseWindow() {
  return native_app_window_.get();
}

gfx::NativeWindow ShellWindow::GetNativeWindow() {
  return GetBaseWindow()->GetNativeWindow();
}

string16 ShellWindow::GetTitle() const {
  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  if (!web_contents() ||
      !web_contents()->GetController().GetActiveEntry() ||
      web_contents()->GetController().GetActiveEntry()->GetTitle().empty())
    return UTF8ToUTF16(extension()->name());
  string16 title = web_contents()->GetTitle();
  Browser::FormatTitleForDisplay(&title);
  return title;
}

void ShellWindow::SetAppIconUrl(const GURL& url) {
  app_icon_url_ = url;
  web_contents()->DownloadFavicon(url, kPreferredIconSize,
                                  base::Bind(&ShellWindow::DidDownloadFavicon,
                                             weak_ptr_factory_.GetWeakPtr()));
}

//------------------------------------------------------------------------------
// Private methods

bool ShellWindow::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellWindow, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShellWindow::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  native_app_window_->UpdateDraggableRegions(regions);
}

void ShellWindow::OnImageLoaded(const gfx::Image& image,
                                const std::string& extension_id,
                                int index) {
  UpdateAppIcon(image);
  app_icon_loader_.reset();
}

void ShellWindow::DidDownloadFavicon(int id,
                                     const GURL& image_url,
                                     bool errored,
                                     int requested_size,
                                     const std::vector<SkBitmap>& bitmaps) {
  if (image_url != app_icon_url_ || bitmaps.empty())
    return;

  // Bitmaps are ordered largest to smallest. Choose the smallest bitmap
  // whose height >= the preferred size.
  int largest_index = 0;
  for (size_t i = 1; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < kPreferredIconSize)
      break;
    largest_index = i;
  }
  const SkBitmap& largest = bitmaps[largest_index];
  UpdateAppIcon(gfx::Image(largest));
}

void ShellWindow::UpdateAppIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return;
  app_icon_ = image;
  native_app_window_->UpdateWindowIcon();
  extensions::ShellWindowRegistry::Get(profile_)->ShellWindowIconChanged(this);
}

void ShellWindow::UpdateExtensionAppIcon() {
  app_icon_loader_.reset(new ImageLoadingTracker(this));
  app_icon_loader_->LoadImage(
      extension(),
      extension()->GetIconResource(kPreferredIconSize,
                                   ExtensionIconSet::MATCH_BIGGER),
      gfx::Size(kPreferredIconSize, kPreferredIconSize),
      ImageLoadingTracker::CACHE);
}

void ShellWindow::CloseContents(WebContents* contents) {
  DCHECK(contents == web_contents_);
  native_app_window_->Close();
}

bool ShellWindow::ShouldSuppressDialogs() {
  return true;
}

void ShellWindow::WebIntentDispatch(
    content::WebContents* web_contents,
    content::WebIntentsDispatcher* intents_dispatcher) {
  if (!web_intents::IsWebIntentsEnabledForProfile(profile_))
    return;

  WebIntentPickerController* web_intent_picker_controller =
      WebIntentPickerController::FromWebContents(web_contents_.get());
  web_intent_picker_controller->SetIntentsDispatcher(intents_dispatcher);
  web_intent_picker_controller->ShowDialog(
      intents_dispatcher->GetIntent().action,
      intents_dispatcher->GetIntent().type);
}

void ShellWindow::RunFileChooser(WebContents* tab,
                                 const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

bool ShellWindow::IsPopupOrPanel(const WebContents* source) const {
  DCHECK(source == web_contents_.get());
  return true;
}

void ShellWindow::MoveContents(WebContents* source, const gfx::Rect& pos) {
  DCHECK(source == web_contents_.get());
  native_app_window_->SetBounds(pos);
}

void ShellWindow::NavigationStateChanged(
    const content::WebContents* source, unsigned changed_flags) {
  DCHECK(source == web_contents_.get());
  if (changed_flags & content::INVALIDATE_TYPE_TITLE)
    native_app_window_->UpdateWindowTitle();
  else if (changed_flags & content::INVALIDATE_TYPE_TAB)
    native_app_window_->UpdateWindowIcon();
}

void ShellWindow::ToggleFullscreenModeForTab(content::WebContents* source,
                                             bool enter_fullscreen) {
  DCHECK(source == web_contents_.get());
  native_app_window_->SetFullscreen(enter_fullscreen);
}

bool ShellWindow::IsFullscreenForTabOrPending(
    const content::WebContents* source) const {
  DCHECK(source == web_contents_.get());
  return native_app_window_->IsFullscreenOrPending();
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      // TODO(jeremya): once http://crbug.com/123007 is fixed, we'll no longer
      // need to suspend resource requests here (the call in the constructor
      // should be enough).
      content::Details<std::pair<RenderViewHost*, RenderViewHost*> >
          host_details(details);
      if (host_details->first)
        SuspendRenderViewHost(host_details->second);
      // TODO(jianli): once http://crbug.com/123007 is fixed, we'll no longer
      // need to make the native window (ShellWindowViews specially) update
      // the clickthrough region for the new RVH.
      native_app_window_->RenderViewHostChanged();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::Extension* unloaded_extension =
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension;
      if (extension_ == unloaded_extension)
        native_app_window_->Close();
      break;
    }
    case chrome::NOTIFICATION_APP_TERMINATING:
      native_app_window_->Close();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

extensions::WindowController*
ShellWindow::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* ShellWindow::GetAssociatedWebContents() const {
  return web_contents_.get();
}

extensions::ActiveTabPermissionGranter*
    ShellWindow::GetActiveTabPermissionGranter() {
  // Shell windows don't support the activeTab permission.
  return NULL;
}

void ShellWindow::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_.Dispatch(params,
                                          web_contents_->GetRenderViewHost());
}

void ShellWindow::AddMessageToDevToolsConsole(ConsoleMessageLevel level,
                                              const std::string& message) {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AddMessageToConsole(
      rvh->GetRoutingID(), level, message));
}

void ShellWindow::SaveWindowPosition() {
  if (window_key_.empty())
    return;
  if (!native_app_window_)
    return;

  extensions::ShellWindowGeometryCache* cache =
      extensions::ExtensionSystem::Get(profile())->
          shell_window_geometry_cache();

  gfx::Rect bounds = native_app_window_->GetBounds();
  cache->SaveGeometry(extension()->id(), window_key_, bounds);
}

// static
SkRegion* ShellWindow::RawDraggableRegionsToSkRegion(
      const std::vector<extensions::DraggableRegion>& regions) {
  SkRegion* sk_region = new SkRegion;
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           regions.begin();
       iter != regions.end(); ++iter) {
    const extensions::DraggableRegion& region = *iter;
    sk_region->op(
        region.bounds.x(),
        region.bounds.y(),
        region.bounds.right(),
        region.bounds.bottom(),
        region.draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  return sk_region;
}
