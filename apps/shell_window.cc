// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell_window.h"

#include "apps/native_app_window.h"
#include "apps/shell_window_geometry_cache.h"
#include "apps/shell_window_registry.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/browser/view_type_utils.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

using content::ConsoleMessageLevel;
using content::WebContents;
using extensions::APIPermission;
using web_modal::WebContentsModalDialogHost;
using web_modal::WebContentsModalDialogManager;

namespace {
const int kDefaultWidth = 512;
const int kDefaultHeight = 384;

}  // namespace

namespace apps {

ShellWindow::CreateParams::CreateParams()
  : window_type(ShellWindow::WINDOW_TYPE_DEFAULT),
    frame(ShellWindow::FRAME_CHROME),
    transparent_background(false),
    bounds(INT_MIN, INT_MIN, 0, 0),
    creator_process_id(0),
    state(ui::SHOW_STATE_DEFAULT),
    hidden(false),
    resizable(true),
    focused(true) {}

ShellWindow::CreateParams::~CreateParams() {}

ShellWindow::Delegate::~Delegate() {}

ShellWindow::ShellWindow(Profile* profile,
                         Delegate* delegate,
                         const extensions::Extension* extension)
    : profile_(profile),
      extension_(extension),
      extension_id_(extension->id()),
      window_type_(WINDOW_TYPE_DEFAULT),
      delegate_(delegate),
      image_loader_ptr_factory_(this),
      fullscreen_for_window_api_(false),
      fullscreen_for_tab_(false) {
}

void ShellWindow::Init(const GURL& url,
                       ShellWindowContents* shell_window_contents,
                       const CreateParams& params) {
  // Initialize the render interface and web contents
  shell_window_contents_.reset(shell_window_contents);
  shell_window_contents_->Initialize(profile(), url);
  WebContents* web_contents = shell_window_contents_->GetWebContents();
  delegate_->InitWebContents(web_contents);
  WebContentsModalDialogManager::CreateForWebContents(web_contents);

  web_contents->SetDelegate(this);
  WebContentsModalDialogManager::FromWebContents(web_contents)->
      set_delegate(this);
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_APP_SHELL);

  // Initialize the window
  window_type_ = params.window_type;

  gfx::Rect bounds = params.bounds;

  if (bounds.width() == 0)
    bounds.set_width(kDefaultWidth);
  if (bounds.height() == 0)
    bounds.set_height(kDefaultHeight);

  // If left and top are left undefined, the native shell window will center
  // the window on the main screen in a platform-defined manner.

  CreateParams new_params = params;

  // Load cached state if it exists.
  if (!params.window_key.empty()) {
    window_key_ = params.window_key;

    ShellWindowGeometryCache* cache = ShellWindowGeometryCache::Get(profile());

    gfx::Rect cached_bounds;
    gfx::Rect cached_screen_bounds;
    ui::WindowShowState cached_state = ui::SHOW_STATE_DEFAULT;
    if (cache->GetGeometry(extension()->id(), params.window_key, &cached_bounds,
                           &cached_screen_bounds, &cached_state)) {
      // App window has cached screen bounds, make sure it fits on screen in
      // case the screen resolution changed.
      gfx::Screen* screen = gfx::Screen::GetNativeScreen();
      gfx::Display display = screen->GetDisplayMatching(cached_bounds);
      gfx::Rect current_screen_bounds = display.work_area();
      AdjustBoundsToBeVisibleOnScreen(cached_bounds,
                                      cached_screen_bounds,
                                      current_screen_bounds,
                                      params.minimum_size,
                                      &bounds);
      new_params.state = cached_state;
    }
  }

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

  native_app_window_.reset(delegate_->CreateNativeAppWindow(this, new_params));

  if (!new_params.hidden) {
    if (window_type_is_panel())
      GetBaseWindow()->ShowInactive();  // Panels are not activated by default.
    else
      GetBaseWindow()->Show();
  }

  if (new_params.state == ui::SHOW_STATE_FULLSCREEN)
    Fullscreen();
  else if (new_params.state == ui::SHOW_STATE_MAXIMIZED)
    Maximize();
  else if (new_params.state == ui::SHOW_STATE_MINIMIZED)
    Minimize();

  OnNativeWindowChanged();

  // When the render view host is changed, the native window needs to know
  // about it in case it has any setup to do to make the renderer appear
  // properly. In particular, on Windows, the view's clickthrough region needs
  // to be set.
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                    &web_contents->GetController()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // Close when the browser process is exiting.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  shell_window_contents_->LoadContents(params.creator_process_id);

  // Prevent the browser process from shutting down while this window is open.
  chrome::StartKeepAlive();

  UpdateExtensionAppIcon();

  ShellWindowRegistry::Get(profile_)->AddShellWindow(this);
}

ShellWindow::~ShellWindow() {
  // Unregister now to prevent getting NOTIFICATION_APP_TERMINATING if we're the
  // last window open.
  registrar_.RemoveAll();

  // Remove shutdown prevention.
  chrome::EndKeepAlive();
}

void ShellWindow::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  delegate_->RequestMediaAccessPermission(web_contents, request, callback,
                                          extension());
}

WebContents* ShellWindow::OpenURLFromTab(WebContents* source,
                                         const content::OpenURLParams& params) {
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

  WebContents* contents = delegate_->OpenURLFromTab(profile_, source,
                                                    params);
  if (!contents) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
  }

  return contents;
}

void ShellWindow::AddNewContents(WebContents* source,
                                 WebContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 bool* was_blocked) {
  DCHECK(Profile::FromBrowserContext(new_contents->GetBrowserContext()) ==
      profile_);
  delegate_->AddNewContents(profile_, new_contents, disposition,
                            initial_pos, user_gesture, was_blocked);
}

void ShellWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
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
  ShellWindowRegistry::Get(profile_)->RemoveShellWindow(this);
  if (shell_window_contents_)
    shell_window_contents_->NativeWindowClosed();
  delete this;
}

void ShellWindow::OnNativeWindowChanged() {
  SaveWindowPosition();
  if (shell_window_contents_ && native_app_window_)
    shell_window_contents_->NativeWindowChanged(native_app_window_.get());
}

void ShellWindow::OnNativeWindowActivated() {
  ShellWindowRegistry::Get(profile_)->ShellWindowActivated(this);
}

scoped_ptr<gfx::Image> ShellWindow::GetAppListIcon() {
  // TODO(skuhne): We might want to use LoadImages in UpdateExtensionAppIcon
  // instead to let the extension give us pre-defined icons in the launcher
  // and the launcher list sizes. Since there is no mock yet, doing this now
  // seems a bit premature and we scale for the time being.
  if (app_icon_.IsEmpty())
    return make_scoped_ptr(new gfx::Image());

  SkBitmap bmp = skia::ImageOperations::Resize(
        *app_icon_.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
        extension_misc::EXTENSION_ICON_SMALLISH,
        extension_misc::EXTENSION_ICON_SMALLISH);
  return make_scoped_ptr(
      new gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bmp)));
}

content::WebContents* ShellWindow::web_contents() const {
  return shell_window_contents_->GetWebContents();
}

NativeAppWindow* ShellWindow::GetBaseWindow() {
  return native_app_window_.get();
}

gfx::NativeWindow ShellWindow::GetNativeWindow() {
  return GetBaseWindow()->GetNativeWindow();
}

gfx::Rect ShellWindow::GetClientBounds() const {
  gfx::Rect bounds = native_app_window_->GetBounds();
  bounds.Inset(native_app_window_->GetFrameInsets());
  return bounds;
}

string16 ShellWindow::GetTitle() const {
  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  string16 title;
  if (!web_contents() ||
      !web_contents()->GetController().GetActiveEntry() ||
      web_contents()->GetController().GetActiveEntry()->GetTitle().empty()) {
    title = UTF8ToUTF16(extension()->name());
  } else {
    title = web_contents()->GetTitle();
  }
  const char16 kBadChars[] = { '\n', 0 };
  RemoveChars(title, kBadChars, &title);
  return title;
}

void ShellWindow::SetAppIconUrl(const GURL& url) {
  // Avoid using any previous app icons were are being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Reset |app_icon_image_| to abort pending image load (if any).
  app_icon_image_.reset();

  app_icon_url_ = url;
  web_contents()->DownloadImage(
      url,
      true,  // is a favicon
      delegate_->PreferredIconSize(),
      0,  // no maximum size
      base::Bind(&ShellWindow::DidDownloadFavicon,
                 image_loader_ptr_factory_.GetWeakPtr()));
}

void ShellWindow::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  native_app_window_->UpdateDraggableRegions(regions);
}

void ShellWindow::UpdateAppIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return;
  app_icon_ = image;
  native_app_window_->UpdateWindowIcon();
  ShellWindowRegistry::Get(profile_)->ShellWindowIconChanged(this);
}

void ShellWindow::Fullscreen() {
  fullscreen_for_window_api_ = true;
  GetBaseWindow()->SetFullscreen(true);
}

void ShellWindow::Maximize() {
  GetBaseWindow()->Maximize();
}

void ShellWindow::Minimize() {
  GetBaseWindow()->Minimize();
}

void ShellWindow::Restore() {
  fullscreen_for_window_api_ = false;
  fullscreen_for_tab_ = false;
  if (GetBaseWindow()->IsFullscreenOrPending()) {
    GetBaseWindow()->SetFullscreen(false);
  } else {
    GetBaseWindow()->Restore();
  }
}

//------------------------------------------------------------------------------
// Private methods

void ShellWindow::DidDownloadFavicon(int id,
                                     int http_status_code,
                                     const GURL& image_url,
                                     int requested_size,
                                     const std::vector<SkBitmap>& bitmaps) {
  if (image_url != app_icon_url_ || bitmaps.empty())
    return;

  // Bitmaps are ordered largest to smallest. Choose the smallest bitmap
  // whose height >= the preferred size.
  int largest_index = 0;
  for (size_t i = 1; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < delegate_->PreferredIconSize())
      break;
    largest_index = i;
  }
  const SkBitmap& largest = bitmaps[largest_index];
  UpdateAppIcon(gfx::Image::CreateFrom1xBitmap(largest));
}

void ShellWindow::OnExtensionIconImageChanged(extensions::IconImage* image) {
  DCHECK_EQ(app_icon_image_.get(), image);

  UpdateAppIcon(gfx::Image(app_icon_image_->image_skia()));
}

void ShellWindow::UpdateExtensionAppIcon() {
  // Avoid using any previous app icons were are being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  app_icon_image_.reset(new extensions::IconImage(
      profile(),
      extension(),
      extensions::IconsInfo::GetIcons(extension()),
      delegate_->PreferredIconSize(),
      extensions::IconsInfo::GetDefaultAppIcon(),
      this));

  // Triggers actual image loading with 1x resources. The 2x resource will
  // be handled by IconImage class when requested.
  app_icon_image_->image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
}

void ShellWindow::CloseContents(WebContents* contents) {
  native_app_window_->Close();
}

bool ShellWindow::ShouldSuppressDialogs() {
  return true;
}

content::ColorChooser* ShellWindow::OpenColorChooser(WebContents* web_contents,
                                                     SkColor initial_color) {
  return delegate_->ShowColorChooser(web_contents, initial_color);
}

void ShellWindow::RunFileChooser(WebContents* tab,
                                 const content::FileChooserParams& params) {
  if (window_type_is_panel()) {
    // Panels can't host a file dialog, abort. TODO(stevenjb): allow file
    // dialogs to be unhosted but still close with the owning web contents.
    // crbug.com/172502.
    LOG(WARNING) << "File dialog opened by panel.";
    return;
  }

  delegate_->RunFileChooser(tab, params);
}

bool ShellWindow::IsPopupOrPanel(const WebContents* source) const {
  return true;
}

void ShellWindow::MoveContents(WebContents* source, const gfx::Rect& pos) {
  native_app_window_->SetBounds(pos);
}

void ShellWindow::NavigationStateChanged(
    const content::WebContents* source, unsigned changed_flags) {
  if (changed_flags & content::INVALIDATE_TYPE_TITLE)
    native_app_window_->UpdateWindowTitle();
  else if (changed_flags & content::INVALIDATE_TYPE_TAB)
    native_app_window_->UpdateWindowIcon();
}

void ShellWindow::ToggleFullscreenModeForTab(content::WebContents* source,
                                             bool enter_fullscreen) {
  if (!IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kFullscreen,
      extension_,
      source->GetRenderViewHost())) {
    return;
  }

  fullscreen_for_tab_ = enter_fullscreen;

  if (enter_fullscreen) {
    native_app_window_->SetFullscreen(true);
  } else if (!fullscreen_for_window_api_) {
    native_app_window_->SetFullscreen(false);
  }
}

bool ShellWindow::IsFullscreenForTabOrPending(
    const content::WebContents* source) const {
  return fullscreen_for_tab_;
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
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

void ShellWindow::SetWebContentsBlocked(content::WebContents* web_contents,
                                        bool blocked) {
  delegate_->SetWebContentsBlocked(web_contents, blocked);
}

bool ShellWindow::IsWebContentsVisible(content::WebContents* web_contents) {
  return delegate_->IsWebContentsVisible(web_contents);
}

extensions::ActiveTabPermissionGranter*
    ShellWindow::GetActiveTabPermissionGranter() {
  // Shell windows don't support the activeTab permission.
  return NULL;
}

WebContentsModalDialogHost* ShellWindow::GetWebContentsModalDialogHost() {
  return native_app_window_.get();
}

void ShellWindow::AddMessageToDevToolsConsole(ConsoleMessageLevel level,
                                              const std::string& message) {
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AddMessageToConsole(
      rvh->GetRoutingID(), level, message));
}

void ShellWindow::SaveWindowPosition() {
  if (window_key_.empty())
    return;
  if (!native_app_window_)
    return;

  ShellWindowGeometryCache* cache = ShellWindowGeometryCache::Get(profile());

  gfx::Rect bounds = native_app_window_->GetRestoredBounds();
  bounds.Inset(native_app_window_->GetFrameInsets());
  gfx::Rect screen_bounds =
      gfx::Screen::GetNativeScreen()->GetDisplayMatching(bounds).work_area();
  ui::WindowShowState window_state = native_app_window_->GetRestoredState();
  cache->SaveGeometry(extension()->id(),
                      window_key_,
                      bounds,
                      screen_bounds,
                      window_state);
}

void ShellWindow::AdjustBoundsToBeVisibleOnScreen(
    const gfx::Rect& cached_bounds,
    const gfx::Rect& cached_screen_bounds,
    const gfx::Rect& current_screen_bounds,
    const gfx::Size& minimum_size,
    gfx::Rect* bounds) const {
  *bounds = cached_bounds;

  // Reposition and resize the bounds if the cached_screen_bounds is different
  // from the current screen bounds and the current screen bounds doesn't
  // completely contain the bounds.
  if (cached_screen_bounds != current_screen_bounds &&
      !current_screen_bounds.Contains(cached_bounds)) {
    bounds->set_width(
        std::max(minimum_size.width(),
                 std::min(bounds->width(), current_screen_bounds.width())));
    bounds->set_height(
        std::max(minimum_size.height(),
                 std::min(bounds->height(), current_screen_bounds.height())));
    bounds->set_x(
        std::max(current_screen_bounds.x(),
                 std::min(bounds->x(),
                          current_screen_bounds.right() - bounds->width())));
    bounds->set_y(
        std::max(current_screen_bounds.y(),
                 std::min(bounds->y(),
                          current_screen_bounds.bottom() - bounds->height())));
  }
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

}  // namespace apps
