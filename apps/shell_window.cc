// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell_window.h"

#include "apps/shell_window_geometry_cache.h"
#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_web_contents_observer.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
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
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/screen.h"

#if !defined(OS_MACOSX)
#include "apps/pref_names.h"
#include "base/prefs/pref_service.h"
#endif

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

ShellWindow::SizeConstraints::SizeConstraints()
    : maximum_size_(kUnboundedSize, kUnboundedSize) {
}

ShellWindow::SizeConstraints::SizeConstraints(const gfx::Size& min_size,
                                              const gfx::Size& max_size)
    : minimum_size_(min_size),
      maximum_size_(max_size) {
}

ShellWindow::SizeConstraints::~SizeConstraints() {}

gfx::Size ShellWindow::SizeConstraints::ClampSize(gfx::Size size) const {
  const gfx::Size max_size = GetMaximumSize();
  if (max_size.width() != kUnboundedSize)
    size.set_width(std::min(size.width(), GetMaximumSize().width()));
  if (max_size.height() != kUnboundedSize)
    size.set_height(std::min(size.height(), GetMaximumSize().height()));
  size.SetToMax(GetMinimumSize());
  return size;
}

bool ShellWindow::SizeConstraints::HasMinimumSize() const {
  return GetMinimumSize().width() != kUnboundedSize ||
      GetMinimumSize().height() != kUnboundedSize;
}

bool ShellWindow::SizeConstraints::HasMaximumSize() const {
  const gfx::Size max_size = GetMaximumSize();
  return max_size.width() != kUnboundedSize ||
      max_size.height() != kUnboundedSize;
}

bool ShellWindow::SizeConstraints::HasFixedSize() const {
  return !GetMinimumSize().IsEmpty() && GetMinimumSize() == GetMaximumSize();
}

gfx::Size ShellWindow::SizeConstraints::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size ShellWindow::SizeConstraints::GetMaximumSize() const {
  return gfx::Size(
      maximum_size_.width() == kUnboundedSize ?
          kUnboundedSize :
          std::max(maximum_size_.width(), minimum_size_.width()),
      maximum_size_.height() == kUnboundedSize ?
          kUnboundedSize :
          std::max(maximum_size_.height(), minimum_size_.height()));
}

void ShellWindow::SizeConstraints::set_minimum_size(const gfx::Size& min_size) {
  minimum_size_ = min_size;
}

void ShellWindow::SizeConstraints::set_maximum_size(const gfx::Size& max_size) {
  maximum_size_ = max_size;
}

ShellWindow::CreateParams::CreateParams()
  : window_type(ShellWindow::WINDOW_TYPE_DEFAULT),
    frame(ShellWindow::FRAME_CHROME),
    transparent_background(false),
    bounds(INT_MIN, INT_MIN, 0, 0),
    creator_process_id(0),
    state(ui::SHOW_STATE_DEFAULT),
    hidden(false),
    resizable(true),
    focused(true),
    always_on_top(false) {}

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
      fullscreen_types_(FULLSCREEN_TYPE_NONE),
      show_on_first_paint_(false),
      first_paint_complete_(false) {
}

void ShellWindow::Init(const GURL& url,
                       ShellWindowContents* shell_window_contents,
                       const CreateParams& params) {
  // Initialize the render interface and web contents
  shell_window_contents_.reset(shell_window_contents);
  shell_window_contents_->Initialize(profile(), url);
  WebContents* web_contents = shell_window_contents_->GetWebContents();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableAppsShowOnFirstPaint)) {
    content::WebContentsObserver::Observe(web_contents);
  }
  delegate_->InitWebContents(web_contents);
  WebContentsModalDialogManager::CreateForWebContents(web_contents);
  extensions::ExtensionWebContentsObserver::CreateForWebContents(web_contents);

  web_contents->SetDelegate(this);
  WebContentsModalDialogManager::FromWebContents(web_contents)->
      SetDelegate(this);
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_APP_SHELL);

  // Initialize the window
  CreateParams new_params = LoadDefaultsAndConstrain(params);
  window_type_ = new_params.window_type;
  window_key_ = new_params.window_key;
  size_constraints_ = SizeConstraints(new_params.minimum_size,
                                      new_params.maximum_size);
  native_app_window_.reset(delegate_->CreateNativeAppWindow(this, new_params));

  if (!new_params.hidden) {
    // Panels are not activated by default.
    Show(window_type_is_panel() ? SHOW_INACTIVE : SHOW_ACTIVE);
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
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // Close when the browser process is exiting.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  shell_window_contents_->LoadContents(new_params.creator_process_id);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableAppsShowOnFirstPaint)) {
    // We want to show the window only when the content has been painted. For
    // that to happen, we need to define a size for the content, otherwise the
    // layout will happen in a 0x0 area.
    // Note: WebContents::GetView() is guaranteed to be non-null.
    web_contents->GetView()->SizeContents(new_params.bounds.size());
  }

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

void ShellWindow::DidFirstVisuallyNonEmptyPaint(int32 page_id) {
  first_paint_complete_ = true;
  if (show_on_first_paint_) {
    DCHECK(delayed_show_type_ == SHOW_ACTIVE ||
           delayed_show_type_ == SHOW_INACTIVE);
    Show(delayed_show_type_);
  }
}

void ShellWindow::OnNativeClose() {
  ShellWindowRegistry::Get(profile_)->RemoveShellWindow(this);
  if (shell_window_contents_) {
    WebContents* web_contents = shell_window_contents_->GetWebContents();
    WebContentsModalDialogManager::FromWebContents(web_contents)->
        SetDelegate(NULL);
    shell_window_contents_->NativeWindowClosed();
  }
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
      0,  // no maximum size
      base::Bind(&ShellWindow::DidDownloadFavicon,
                 image_loader_ptr_factory_.GetWeakPtr()));
}

void ShellWindow::UpdateInputRegion(scoped_ptr<SkRegion> region) {
  native_app_window_->UpdateInputRegion(region.Pass());
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
#if !defined(OS_MACOSX)
  // Do not enter fullscreen mode if disallowed by pref.
  if (!profile()->GetPrefs()->GetBoolean(prefs::kAppFullscreenAllowed))
    return;
#endif
  fullscreen_types_ |= FULLSCREEN_TYPE_WINDOW_API;
  GetBaseWindow()->SetFullscreen(fullscreen_types_);
}

void ShellWindow::Maximize() {
  GetBaseWindow()->Maximize();
}

void ShellWindow::Minimize() {
  GetBaseWindow()->Minimize();
}

void ShellWindow::Restore() {
  if (fullscreen_types_ != FULLSCREEN_TYPE_NONE) {
    fullscreen_types_ = FULLSCREEN_TYPE_NONE;
    GetBaseWindow()->SetFullscreen(fullscreen_types_);
  } else {
    GetBaseWindow()->Restore();
  }
}

void ShellWindow::OSFullscreen() {
#if !defined(OS_MACOSX)
  // Do not enter fullscreen mode if disallowed by pref.
  if (!profile()->GetPrefs()->GetBoolean(prefs::kAppFullscreenAllowed))
    return;
#endif
  fullscreen_types_ |= FULLSCREEN_TYPE_OS;
  GetBaseWindow()->SetFullscreen(fullscreen_types_);
}

void ShellWindow::SetMinimumSize(const gfx::Size& min_size) {
  size_constraints_.set_minimum_size(min_size);
  OnSizeConstraintsChanged();
}

void ShellWindow::SetMaximumSize(const gfx::Size& max_size) {
  size_constraints_.set_maximum_size(max_size);
  OnSizeConstraintsChanged();
}

void ShellWindow::Show(ShowType show_type) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableAppsShowOnFirstPaint)) {
    show_on_first_paint_ = true;

    if (!first_paint_complete_) {
      delayed_show_type_ = show_type;
      return;
    }
  }

  switch (show_type) {
    case SHOW_ACTIVE:
      GetBaseWindow()->Show();
      break;
    case SHOW_INACTIVE:
      GetBaseWindow()->ShowInactive();
      break;
  }
}

void ShellWindow::Hide() {
  // This is there to prevent race conditions with Hide() being called before
  // there was a non-empty paint. It should have no effect in a non-racy
  // scenario where the application is hiding then showing a window: the second
  // show will not be delayed.
  show_on_first_paint_ = false;
  GetBaseWindow()->Hide();
}

//------------------------------------------------------------------------------
// Private methods

void ShellWindow::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
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
  app_icon_image_->image_skia().GetRepresentation(1.0f);
}

void ShellWindow::OnSizeConstraintsChanged() {
  native_app_window_->UpdateWindowMinMaxSize();
  gfx::Rect bounds = GetClientBounds();
  gfx::Size constrained_size = size_constraints_.ClampSize(bounds.size());
  if (bounds.size() != constrained_size) {
    bounds.set_size(constrained_size);
    native_app_window_->SetBounds(bounds);
  }
  OnNativeWindowChanged();
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
#if !defined(OS_MACOSX)
  // Do not enter fullscreen mode if disallowed by pref.
  // TODO(bartfab): Add a test once it becomes possible to simulate a user
  // gesture. http://crbug.com/174178
  if (enter_fullscreen &&
      !profile()->GetPrefs()->GetBoolean(prefs::kAppFullscreenAllowed)) {
    return;
  }
#endif

  if (!IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kFullscreen,
      extension_,
      source->GetRenderViewHost())) {
    return;
  }

  if (enter_fullscreen)
    fullscreen_types_ |= FULLSCREEN_TYPE_HTML_API;
  else
    fullscreen_types_ &= ~FULLSCREEN_TYPE_HTML_API;
  GetBaseWindow()->SetFullscreen(fullscreen_types_);
}

bool ShellWindow::IsFullscreenForTabOrPending(
    const content::WebContents* source) const {
  return ((fullscreen_types_ & FULLSCREEN_TYPE_HTML_API) != 0);
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
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

ShellWindow::CreateParams ShellWindow::LoadDefaultsAndConstrain(
    CreateParams params) const {
  if (params.bounds.width() == 0)
    params.bounds.set_width(kDefaultWidth);
  if (params.bounds.height() == 0)
    params.bounds.set_height(kDefaultHeight);

  // If left and top are left undefined, the native shell window will center
  // the window on the main screen in a platform-defined manner.

  // Load cached state if it exists.
  if (!params.window_key.empty()) {
    ShellWindowGeometryCache* cache = ShellWindowGeometryCache::Get(profile());

    gfx::Rect cached_bounds;
    gfx::Rect cached_screen_bounds;
    ui::WindowShowState cached_state = ui::SHOW_STATE_DEFAULT;
    if (cache->GetGeometry(extension()->id(), params.window_key,
                           &cached_bounds, &cached_screen_bounds,
                           &cached_state)) {
      // App window has cached screen bounds, make sure it fits on screen in
      // case the screen resolution changed.
      gfx::Screen* screen = gfx::Screen::GetNativeScreen();
      gfx::Display display = screen->GetDisplayMatching(cached_bounds);
      gfx::Rect current_screen_bounds = display.work_area();
      AdjustBoundsToBeVisibleOnScreen(cached_bounds,
                                      cached_screen_bounds,
                                      current_screen_bounds,
                                      params.minimum_size,
                                      &params.bounds);
      params.state = cached_state;
    }
  }

  SizeConstraints size_constraints(params.minimum_size, params.maximum_size);
  params.bounds.set_size(size_constraints.ClampSize(params.bounds.size()));
  params.minimum_size = size_constraints.GetMinimumSize();
  params.maximum_size = size_constraints.GetMaximumSize();

  return params;
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
