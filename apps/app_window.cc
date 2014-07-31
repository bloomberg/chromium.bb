// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"

#include <algorithm>
#include <string>
#include <vector>

#include "apps/app_delegate.h"
#include "apps/app_window_geometry_cache.h"
#include "apps/app_window_registry.h"
#include "apps/apps_client.h"
#include "apps/size_constraints.h"
#include "apps/ui/native_app_window.h"
#include "apps/ui/web_contents_sizer.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"

#if !defined(OS_MACOSX)
#include "apps/pref_names.h"
#include "base/prefs/pref_service.h"
#endif

using content::BrowserContext;
using content::ConsoleMessageLevel;
using content::WebContents;
using extensions::APIPermission;
using web_modal::WebContentsModalDialogHost;
using web_modal::WebContentsModalDialogManager;

namespace apps {

namespace {

const int kDefaultWidth = 512;
const int kDefaultHeight = 384;

void SetConstraintProperty(const std::string& name,
                           int value,
                           base::DictionaryValue* bounds_properties) {
  if (value != SizeConstraints::kUnboundedSize)
    bounds_properties->SetInteger(name, value);
  else
    bounds_properties->Set(name, base::Value::CreateNullValue());
}

void SetBoundsProperties(const gfx::Rect& bounds,
                         const gfx::Size& min_size,
                         const gfx::Size& max_size,
                         const std::string& bounds_name,
                         base::DictionaryValue* window_properties) {
  scoped_ptr<base::DictionaryValue> bounds_properties(
      new base::DictionaryValue());

  bounds_properties->SetInteger("left", bounds.x());
  bounds_properties->SetInteger("top", bounds.y());
  bounds_properties->SetInteger("width", bounds.width());
  bounds_properties->SetInteger("height", bounds.height());

  SetConstraintProperty("minWidth", min_size.width(), bounds_properties.get());
  SetConstraintProperty(
      "minHeight", min_size.height(), bounds_properties.get());
  SetConstraintProperty("maxWidth", max_size.width(), bounds_properties.get());
  SetConstraintProperty(
      "maxHeight", max_size.height(), bounds_properties.get());

  window_properties->Set(bounds_name, bounds_properties.release());
}

// Combines the constraints of the content and window, and returns constraints
// for the window.
gfx::Size GetCombinedWindowConstraints(const gfx::Size& window_constraints,
                                       const gfx::Size& content_constraints,
                                       const gfx::Insets& frame_insets) {
  gfx::Size combined_constraints(window_constraints);
  if (content_constraints.width() > 0) {
    combined_constraints.set_width(
        content_constraints.width() + frame_insets.width());
  }
  if (content_constraints.height() > 0) {
    combined_constraints.set_height(
        content_constraints.height() + frame_insets.height());
  }
  return combined_constraints;
}

// Combines the constraints of the content and window, and returns constraints
// for the content.
gfx::Size GetCombinedContentConstraints(const gfx::Size& window_constraints,
                                        const gfx::Size& content_constraints,
                                        const gfx::Insets& frame_insets) {
  gfx::Size combined_constraints(content_constraints);
  if (window_constraints.width() > 0) {
    combined_constraints.set_width(
        std::max(0, window_constraints.width() - frame_insets.width()));
  }
  if (window_constraints.height() > 0) {
    combined_constraints.set_height(
        std::max(0, window_constraints.height() - frame_insets.height()));
  }
  return combined_constraints;
}

}  // namespace

// AppWindow::BoundsSpecification

const int AppWindow::BoundsSpecification::kUnspecifiedPosition = INT_MIN;

AppWindow::BoundsSpecification::BoundsSpecification()
    : bounds(kUnspecifiedPosition, kUnspecifiedPosition, 0, 0) {}

AppWindow::BoundsSpecification::~BoundsSpecification() {}

void AppWindow::BoundsSpecification::ResetBounds() {
  bounds.SetRect(kUnspecifiedPosition, kUnspecifiedPosition, 0, 0);
}

// AppWindow::CreateParams

AppWindow::CreateParams::CreateParams()
    : window_type(AppWindow::WINDOW_TYPE_DEFAULT),
      frame(AppWindow::FRAME_CHROME),
      has_frame_color(false),
      active_frame_color(SK_ColorBLACK),
      inactive_frame_color(SK_ColorBLACK),
      transparent_background(false),
      creator_process_id(0),
      state(ui::SHOW_STATE_DEFAULT),
      hidden(false),
      resizable(true),
      focused(true),
      always_on_top(false) {}

AppWindow::CreateParams::~CreateParams() {}

gfx::Rect AppWindow::CreateParams::GetInitialWindowBounds(
    const gfx::Insets& frame_insets) const {
  // Combine into a single window bounds.
  gfx::Rect combined_bounds(window_spec.bounds);
  if (content_spec.bounds.x() != BoundsSpecification::kUnspecifiedPosition)
    combined_bounds.set_x(content_spec.bounds.x() - frame_insets.left());
  if (content_spec.bounds.y() != BoundsSpecification::kUnspecifiedPosition)
    combined_bounds.set_y(content_spec.bounds.y() - frame_insets.top());
  if (content_spec.bounds.width() > 0) {
    combined_bounds.set_width(
        content_spec.bounds.width() + frame_insets.width());
  }
  if (content_spec.bounds.height() > 0) {
    combined_bounds.set_height(
        content_spec.bounds.height() + frame_insets.height());
  }

  // Constrain the bounds.
  SizeConstraints constraints(
      GetCombinedWindowConstraints(
          window_spec.minimum_size, content_spec.minimum_size, frame_insets),
      GetCombinedWindowConstraints(
          window_spec.maximum_size, content_spec.maximum_size, frame_insets));
  combined_bounds.set_size(constraints.ClampSize(combined_bounds.size()));

  return combined_bounds;
}

gfx::Size AppWindow::CreateParams::GetContentMinimumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedContentConstraints(window_spec.minimum_size,
                                       content_spec.minimum_size,
                                       frame_insets);
}

gfx::Size AppWindow::CreateParams::GetContentMaximumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedContentConstraints(window_spec.maximum_size,
                                       content_spec.maximum_size,
                                       frame_insets);
}

gfx::Size AppWindow::CreateParams::GetWindowMinimumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedWindowConstraints(window_spec.minimum_size,
                                      content_spec.minimum_size,
                                      frame_insets);
}

gfx::Size AppWindow::CreateParams::GetWindowMaximumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedWindowConstraints(window_spec.maximum_size,
                                      content_spec.maximum_size,
                                      frame_insets);
}

// AppWindow::Delegate

AppWindow::Delegate::~Delegate() {}

// AppWindow

AppWindow::AppWindow(BrowserContext* context,
                     AppDelegate* app_delegate,
                     Delegate* delegate,
                     const extensions::Extension* extension)
    : browser_context_(context),
      extension_id_(extension->id()),
      window_type_(WINDOW_TYPE_DEFAULT),
      app_delegate_(app_delegate),
      delegate_(delegate),
      image_loader_ptr_factory_(this),
      fullscreen_types_(FULLSCREEN_TYPE_NONE),
      show_on_first_paint_(false),
      first_paint_complete_(false),
      has_been_shown_(false),
      can_send_events_(false),
      is_hidden_(false),
      cached_always_on_top_(false),
      requested_transparent_background_(false) {
  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  CHECK(!client->IsGuestSession(context) || context->IsOffTheRecord())
      << "Only off the record window may be opened in the guest mode.";
}

void AppWindow::Init(const GURL& url,
                     AppWindowContents* app_window_contents,
                     const CreateParams& params) {
  // Initialize the render interface and web contents
  app_window_contents_.reset(app_window_contents);
  app_window_contents_->Initialize(browser_context(), url);
  WebContents* web_contents = app_window_contents_->GetWebContents();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAppsShowOnFirstPaint)) {
    content::WebContentsObserver::Observe(web_contents);
  }
  app_delegate_->InitWebContents(web_contents);

  WebContentsModalDialogManager::CreateForWebContents(web_contents);
  // TODO(jamescook): Delegate out this creation.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);

  web_contents->SetDelegate(this);
  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_APP_WINDOW);

  // Initialize the window
  CreateParams new_params = LoadDefaults(params);
  window_type_ = new_params.window_type;
  window_key_ = new_params.window_key;

  // Windows cannot be always-on-top in fullscreen mode for security reasons.
  cached_always_on_top_ = new_params.always_on_top;
  if (new_params.state == ui::SHOW_STATE_FULLSCREEN)
    new_params.always_on_top = false;

  requested_transparent_background_ = new_params.transparent_background;

  native_app_window_.reset(delegate_->CreateNativeAppWindow(this, new_params));

  popup_manager_.reset(
      new web_modal::PopupManager(GetWebContentsModalDialogHost()));
  popup_manager_->RegisterWith(web_contents);

  // Prevent the browser process from shutting down while this window exists.
  AppsClient::Get()->IncrementKeepAliveCount();
  UpdateExtensionAppIcon();
  AppWindowRegistry::Get(browser_context_)->AddAppWindow(this);

  if (new_params.hidden) {
    // Although the window starts hidden by default, calling Hide() here
    // notifies observers of the window being hidden.
    Hide();
  } else {
    // Panels are not activated by default.
    Show(window_type_is_panel() || !new_params.focused ? SHOW_INACTIVE
                                                       : SHOW_ACTIVE);
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
  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<content::BrowserContext>(
                     client->GetOriginalContext(browser_context_)));
  // Close when the browser process is exiting.
  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  // Update the app menu if an ephemeral app becomes installed.
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::Source<content::BrowserContext>(
          client->GetOriginalContext(browser_context_)));

  app_window_contents_->LoadContents(new_params.creator_process_id);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAppsShowOnFirstPaint)) {
    // We want to show the window only when the content has been painted. For
    // that to happen, we need to define a size for the content, otherwise the
    // layout will happen in a 0x0 area.
    gfx::Insets frame_insets = native_app_window_->GetFrameInsets();
    gfx::Rect initial_bounds = new_params.GetInitialWindowBounds(frame_insets);
    initial_bounds.Inset(frame_insets);
    apps::ResizeWebContents(web_contents, initial_bounds.size());
  }
}

AppWindow::~AppWindow() {
  // Unregister now to prevent getting NOTIFICATION_APP_TERMINATING if we're the
  // last window open.
  registrar_.RemoveAll();

  // Remove shutdown prevention.
  AppsClient::Get()->DecrementKeepAliveCount();
}

void AppWindow::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  app_delegate_->RequestMediaAccessPermission(
      web_contents, request, callback, extension);
}

WebContents* AppWindow::OpenURLFromTab(WebContents* source,
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

  WebContents* contents =
      app_delegate_->OpenURLFromTab(browser_context_, source, params);
  if (!contents) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
  }

  return contents;
}

void AppWindow::AddNewContents(WebContents* source,
                               WebContents* new_contents,
                               WindowOpenDisposition disposition,
                               const gfx::Rect& initial_pos,
                               bool user_gesture,
                               bool* was_blocked) {
  DCHECK(new_contents->GetBrowserContext() == browser_context_);
  app_delegate_->AddNewContents(browser_context_,
                                new_contents,
                                disposition,
                                initial_pos,
                                user_gesture,
                                was_blocked);
}

bool AppWindow::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return false;

  // Here, we can handle a key event before the content gets it. When we are
  // fullscreen and it is not forced, we want to allow the user to leave
  // when ESC is pressed.
  // However, if the application has the "overrideEscFullscreen" permission, we
  // should let it override that behavior.
  // ::HandleKeyboardEvent() will only be called if the KeyEvent's default
  // action is not prevented.
  // Thus, we should handle the KeyEvent here only if the permission is not set.
  if (event.windowsKeyCode == ui::VKEY_ESCAPE && IsFullscreen() &&
      !IsForcedFullscreen() &&
      !extension->permissions_data()->HasAPIPermission(
          APIPermission::kOverrideEscFullscreen)) {
    Restore();
    return true;
  }

  return false;
}

void AppWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // If the window is currently fullscreen and not forced, ESC should leave
  // fullscreen.  If this code is being called for ESC, that means that the
  // KeyEvent's default behavior was not prevented by the content.
  if (event.windowsKeyCode == ui::VKEY_ESCAPE && IsFullscreen() &&
      !IsForcedFullscreen()) {
    Restore();
    return;
  }

  native_app_window_->HandleKeyboardEvent(event);
}

void AppWindow::RequestToLockMouse(WebContents* web_contents,
                                   bool user_gesture,
                                   bool last_unlocked_by_target) {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  bool has_permission = IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kPointerLock,
      extension,
      web_contents->GetRenderViewHost());

  web_contents->GotResponseToLockMouseRequest(has_permission);
}

bool AppWindow::PreHandleGestureEvent(WebContents* source,
                                      const blink::WebGestureEvent& event) {
  // Disable pinch zooming in app windows.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
         event.type == blink::WebGestureEvent::GesturePinchUpdate ||
         event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void AppWindow::DidFirstVisuallyNonEmptyPaint() {
  first_paint_complete_ = true;
  if (show_on_first_paint_) {
    DCHECK(delayed_show_type_ == SHOW_ACTIVE ||
           delayed_show_type_ == SHOW_INACTIVE);
    Show(delayed_show_type_);
  }
}

void AppWindow::OnNativeClose() {
  AppWindowRegistry::Get(browser_context_)->RemoveAppWindow(this);
  if (app_window_contents_) {
    WebContents* web_contents = app_window_contents_->GetWebContents();
    WebContentsModalDialogManager::FromWebContents(web_contents)
        ->SetDelegate(NULL);
    app_window_contents_->NativeWindowClosed();
  }
  delete this;
}

void AppWindow::OnNativeWindowChanged() {
  SaveWindowPosition();

#if defined(OS_WIN)
  if (native_app_window_ && cached_always_on_top_ && !IsFullscreen() &&
      !native_app_window_->IsMaximized() &&
      !native_app_window_->IsMinimized()) {
    UpdateNativeAlwaysOnTop();
  }
#endif

  if (app_window_contents_ && native_app_window_)
    app_window_contents_->NativeWindowChanged(native_app_window_.get());
}

void AppWindow::OnNativeWindowActivated() {
  AppWindowRegistry::Get(browser_context_)->AppWindowActivated(this);
}

content::WebContents* AppWindow::web_contents() const {
  return app_window_contents_->GetWebContents();
}

const extensions::Extension* AppWindow::GetExtension() const {
  return extensions::ExtensionRegistry::Get(browser_context_)
      ->enabled_extensions()
      .GetByID(extension_id_);
}

NativeAppWindow* AppWindow::GetBaseWindow() { return native_app_window_.get(); }

gfx::NativeWindow AppWindow::GetNativeWindow() {
  return GetBaseWindow()->GetNativeWindow();
}

gfx::Rect AppWindow::GetClientBounds() const {
  gfx::Rect bounds = native_app_window_->GetBounds();
  bounds.Inset(native_app_window_->GetFrameInsets());
  return bounds;
}

base::string16 AppWindow::GetTitle() const {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return base::string16();

  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  base::string16 title;
  if (!web_contents() || !web_contents()->GetController().GetActiveEntry() ||
      web_contents()->GetController().GetActiveEntry()->GetTitle().empty()) {
    title = base::UTF8ToUTF16(extension->name());
  } else {
    title = web_contents()->GetTitle();
  }
  base::RemoveChars(title, base::ASCIIToUTF16("\n"), &title);
  return title;
}

void AppWindow::SetAppIconUrl(const GURL& url) {
  // If the same url is being used for the badge, ignore it.
  if (url == badge_icon_url_)
    return;

  // Avoid using any previous icons that were being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Reset |app_icon_image_| to abort pending image load (if any).
  app_icon_image_.reset();

  app_icon_url_ = url;
  web_contents()->DownloadImage(
      url,
      true,  // is a favicon
      0,     // no maximum size
      base::Bind(&AppWindow::DidDownloadFavicon,
                 image_loader_ptr_factory_.GetWeakPtr()));
}

void AppWindow::SetBadgeIconUrl(const GURL& url) {
  // Avoid using any previous icons that were being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Reset |app_icon_image_| to abort pending image load (if any).
  badge_icon_image_.reset();

  badge_icon_url_ = url;
  web_contents()->DownloadImage(
      url,
      true,  // is a favicon
      0,     // no maximum size
      base::Bind(&AppWindow::DidDownloadFavicon,
                 image_loader_ptr_factory_.GetWeakPtr()));
}

void AppWindow::ClearBadge() {
  badge_icon_image_.reset();
  badge_icon_url_ = GURL();
  UpdateBadgeIcon(gfx::Image());
}

void AppWindow::UpdateShape(scoped_ptr<SkRegion> region) {
  native_app_window_->UpdateShape(region.Pass());
}

void AppWindow::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  native_app_window_->UpdateDraggableRegions(regions);
}

void AppWindow::UpdateAppIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return;
  app_icon_ = image;
  native_app_window_->UpdateWindowIcon();
  AppWindowRegistry::Get(browser_context_)->AppWindowIconChanged(this);
}

void AppWindow::SetFullscreen(FullscreenType type, bool enable) {
  DCHECK_NE(FULLSCREEN_TYPE_NONE, type);

  if (enable) {
#if !defined(OS_MACOSX)
    // Do not enter fullscreen mode if disallowed by pref.
    // TODO(bartfab): Add a test once it becomes possible to simulate a user
    // gesture. http://crbug.com/174178
    if (type != FULLSCREEN_TYPE_FORCED) {
      PrefService* prefs =
          extensions::ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
              browser_context());
      if (!prefs->GetBoolean(prefs::kAppFullscreenAllowed))
        return;
    }
#endif
    fullscreen_types_ |= type;
  } else {
    fullscreen_types_ &= ~type;
  }
  SetNativeWindowFullscreen();
}

bool AppWindow::IsFullscreen() const {
  return fullscreen_types_ != FULLSCREEN_TYPE_NONE;
}

bool AppWindow::IsForcedFullscreen() const {
  return (fullscreen_types_ & FULLSCREEN_TYPE_FORCED) != 0;
}

bool AppWindow::IsHtmlApiFullscreen() const {
  return (fullscreen_types_ & FULLSCREEN_TYPE_HTML_API) != 0;
}

void AppWindow::Fullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_WINDOW_API, true);
}

void AppWindow::Maximize() { GetBaseWindow()->Maximize(); }

void AppWindow::Minimize() { GetBaseWindow()->Minimize(); }

void AppWindow::Restore() {
  if (IsFullscreen()) {
    fullscreen_types_ = FULLSCREEN_TYPE_NONE;
    SetNativeWindowFullscreen();
  } else {
    GetBaseWindow()->Restore();
  }
}

void AppWindow::OSFullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_OS, true);
}

void AppWindow::ForcedFullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_FORCED, true);
}

void AppWindow::SetContentSizeConstraints(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SizeConstraints constraints(min_size, max_size);
  native_app_window_->SetContentSizeConstraints(constraints.GetMinimumSize(),
                                                constraints.GetMaximumSize());

  gfx::Rect bounds = GetClientBounds();
  gfx::Size constrained_size = constraints.ClampSize(bounds.size());
  if (bounds.size() != constrained_size) {
    bounds.set_size(constrained_size);
    bounds.Inset(-native_app_window_->GetFrameInsets());
    native_app_window_->SetBounds(bounds);
  }
  OnNativeWindowChanged();
}

void AppWindow::Show(ShowType show_type) {
  is_hidden_ = false;

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
  AppWindowRegistry::Get(browser_context_)->AppWindowShown(this);

  has_been_shown_ = true;
  SendOnWindowShownIfShown();
}

void AppWindow::Hide() {
  // This is there to prevent race conditions with Hide() being called before
  // there was a non-empty paint. It should have no effect in a non-racy
  // scenario where the application is hiding then showing a window: the second
  // show will not be delayed.
  is_hidden_ = true;
  show_on_first_paint_ = false;
  GetBaseWindow()->Hide();
  AppWindowRegistry::Get(browser_context_)->AppWindowHidden(this);
}

void AppWindow::SetAlwaysOnTop(bool always_on_top) {
  if (cached_always_on_top_ == always_on_top)
    return;

  cached_always_on_top_ = always_on_top;

  // As a security measure, do not allow fullscreen windows or windows that
  // overlap the taskbar to be on top. The property will be applied when the
  // window exits fullscreen and moves away from the taskbar.
  if (!IsFullscreen() && !IntersectsWithTaskbar())
    native_app_window_->SetAlwaysOnTop(always_on_top);

  OnNativeWindowChanged();
}

bool AppWindow::IsAlwaysOnTop() const { return cached_always_on_top_; }

void AppWindow::WindowEventsReady() {
  can_send_events_ = true;
  SendOnWindowShownIfShown();
}

void AppWindow::GetSerializedState(base::DictionaryValue* properties) const {
  DCHECK(properties);

  properties->SetBoolean("fullscreen",
                         native_app_window_->IsFullscreenOrPending());
  properties->SetBoolean("minimized", native_app_window_->IsMinimized());
  properties->SetBoolean("maximized", native_app_window_->IsMaximized());
  properties->SetBoolean("alwaysOnTop", IsAlwaysOnTop());
  properties->SetBoolean("hasFrameColor", native_app_window_->HasFrameColor());
  properties->SetBoolean("alphaEnabled",
                         requested_transparent_background_ &&
                             native_app_window_->CanHaveAlphaEnabled());

  // These properties are undocumented and are to enable testing. Alpha is
  // removed to
  // make the values easier to check.
  SkColor transparent_white = ~SK_ColorBLACK;
  properties->SetInteger(
      "activeFrameColor",
      native_app_window_->ActiveFrameColor() & transparent_white);
  properties->SetInteger(
      "inactiveFrameColor",
      native_app_window_->InactiveFrameColor() & transparent_white);

  gfx::Rect content_bounds = GetClientBounds();
  gfx::Size content_min_size = native_app_window_->GetContentMinimumSize();
  gfx::Size content_max_size = native_app_window_->GetContentMaximumSize();
  SetBoundsProperties(content_bounds,
                      content_min_size,
                      content_max_size,
                      "innerBounds",
                      properties);

  gfx::Insets frame_insets = native_app_window_->GetFrameInsets();
  gfx::Rect frame_bounds = native_app_window_->GetBounds();
  gfx::Size frame_min_size =
      SizeConstraints::AddFrameToConstraints(content_min_size, frame_insets);
  gfx::Size frame_max_size =
      SizeConstraints::AddFrameToConstraints(content_max_size, frame_insets);
  SetBoundsProperties(frame_bounds,
                      frame_min_size,
                      frame_max_size,
                      "outerBounds",
                      properties);
}

//------------------------------------------------------------------------------
// Private methods

void AppWindow::UpdateBadgeIcon(const gfx::Image& image) {
  badge_icon_ = image;
  native_app_window_->UpdateBadgeIcon();
}

void AppWindow::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if ((image_url != app_icon_url_ && image_url != badge_icon_url_) ||
      bitmaps.empty()) {
    return;
  }

  // Bitmaps are ordered largest to smallest. Choose the smallest bitmap
  // whose height >= the preferred size.
  int largest_index = 0;
  for (size_t i = 1; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < app_delegate_->PreferredIconSize())
      break;
    largest_index = i;
  }
  const SkBitmap& largest = bitmaps[largest_index];
  if (image_url == app_icon_url_) {
    UpdateAppIcon(gfx::Image::CreateFrom1xBitmap(largest));
    return;
  }

  UpdateBadgeIcon(gfx::Image::CreateFrom1xBitmap(largest));
}

void AppWindow::OnExtensionIconImageChanged(extensions::IconImage* image) {
  DCHECK_EQ(app_icon_image_.get(), image);

  UpdateAppIcon(gfx::Image(app_icon_image_->image_skia()));
}

void AppWindow::UpdateExtensionAppIcon() {
  // Avoid using any previous app icons were being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  const gfx::ImageSkia& default_icon =
      *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_APP_DEFAULT_ICON);

  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  app_icon_image_.reset(
      new extensions::IconImage(browser_context(),
                                extension,
                                extensions::IconsInfo::GetIcons(extension),
                                app_delegate_->PreferredIconSize(),
                                default_icon,
                                this));

  // Triggers actual image loading with 1x resources. The 2x resource will
  // be handled by IconImage class when requested.
  app_icon_image_->image_skia().GetRepresentation(1.0f);
}

void AppWindow::SetNativeWindowFullscreen() {
  native_app_window_->SetFullscreen(fullscreen_types_);

  if (cached_always_on_top_)
    UpdateNativeAlwaysOnTop();
}

bool AppWindow::IntersectsWithTaskbar() const {
#if defined(OS_WIN)
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Rect window_bounds = native_app_window_->GetRestoredBounds();
  std::vector<gfx::Display> displays = screen->GetAllDisplays();

  for (std::vector<gfx::Display>::const_iterator it = displays.begin();
       it != displays.end();
       ++it) {
    gfx::Rect taskbar_bounds = it->bounds();
    taskbar_bounds.Subtract(it->work_area());
    if (taskbar_bounds.IsEmpty())
      continue;

    if (window_bounds.Intersects(taskbar_bounds))
      return true;
  }
#endif

  return false;
}

void AppWindow::UpdateNativeAlwaysOnTop() {
  DCHECK(cached_always_on_top_);
  bool is_on_top = native_app_window_->IsAlwaysOnTop();
  bool fullscreen = IsFullscreen();
  bool intersects_taskbar = IntersectsWithTaskbar();

  if (is_on_top && (fullscreen || intersects_taskbar)) {
    // When entering fullscreen or overlapping the taskbar, ensure windows are
    // not always-on-top.
    native_app_window_->SetAlwaysOnTop(false);
  } else if (!is_on_top && !fullscreen && !intersects_taskbar) {
    // When exiting fullscreen and moving away from the taskbar, reinstate
    // always-on-top.
    native_app_window_->SetAlwaysOnTop(true);
  }
}

void AppWindow::SendOnWindowShownIfShown() {
  if (!can_send_events_ || !has_been_shown_)
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    app_window_contents_->DispatchWindowShownForTests();
  }
}

void AppWindow::CloseContents(WebContents* contents) {
  native_app_window_->Close();
}

bool AppWindow::ShouldSuppressDialogs() { return true; }

content::ColorChooser* AppWindow::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  return app_delegate_->ShowColorChooser(web_contents, initial_color);
}

void AppWindow::RunFileChooser(WebContents* tab,
                               const content::FileChooserParams& params) {
  if (window_type_is_panel()) {
    // Panels can't host a file dialog, abort. TODO(stevenjb): allow file
    // dialogs to be unhosted but still close with the owning web contents.
    // crbug.com/172502.
    LOG(WARNING) << "File dialog opened by panel.";
    return;
  }

  app_delegate_->RunFileChooser(tab, params);
}

bool AppWindow::IsPopupOrPanel(const WebContents* source) const { return true; }

void AppWindow::MoveContents(WebContents* source, const gfx::Rect& pos) {
  native_app_window_->SetBounds(pos);
}

void AppWindow::NavigationStateChanged(const content::WebContents* source,
                                       unsigned changed_flags) {
  if (changed_flags & content::INVALIDATE_TYPE_TITLE)
    native_app_window_->UpdateWindowTitle();
  else if (changed_flags & content::INVALIDATE_TYPE_TAB)
    native_app_window_->UpdateWindowIcon();
}

void AppWindow::ToggleFullscreenModeForTab(content::WebContents* source,
                                           bool enter_fullscreen) {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  if (!IsExtensionWithPermissionOrSuggestInConsole(
          APIPermission::kFullscreen, extension, source->GetRenderViewHost())) {
    return;
  }

  SetFullscreen(FULLSCREEN_TYPE_HTML_API, enter_fullscreen);
}

bool AppWindow::IsFullscreenForTabOrPending(const content::WebContents* source)
    const {
  return IsHtmlApiFullscreen();
}

void AppWindow::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const extensions::Extension* unloaded_extension =
          content::Details<extensions::UnloadedExtensionInfo>(details)
              ->extension;
      if (extension_id_ == unloaded_extension->id())
        native_app_window_->Close();
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED: {
      const extensions::Extension* installed_extension =
          content::Details<const extensions::InstalledExtensionInfo>(details)
              ->extension;
      DCHECK(installed_extension);
      if (installed_extension->id() == extension_id())
        native_app_window_->UpdateShelfMenu();
      break;
    }
    case chrome::NOTIFICATION_APP_TERMINATING:
      native_app_window_->Close();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

void AppWindow::SetWebContentsBlocked(content::WebContents* web_contents,
                                      bool blocked) {
  app_delegate_->SetWebContentsBlocked(web_contents, blocked);
}

bool AppWindow::IsWebContentsVisible(content::WebContents* web_contents) {
  return app_delegate_->IsWebContentsVisible(web_contents);
}

WebContentsModalDialogHost* AppWindow::GetWebContentsModalDialogHost() {
  return native_app_window_.get();
}

void AppWindow::AddMessageToDevToolsConsole(ConsoleMessageLevel level,
                                            const std::string& message) {
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AddMessageToConsole(
      rvh->GetRoutingID(), level, message));
}

void AppWindow::SaveWindowPosition() {
  if (window_key_.empty())
    return;
  if (!native_app_window_)
    return;

  AppWindowGeometryCache* cache =
      AppWindowGeometryCache::Get(browser_context());

  gfx::Rect bounds = native_app_window_->GetRestoredBounds();
  gfx::Rect screen_bounds =
      gfx::Screen::GetNativeScreen()->GetDisplayMatching(bounds).work_area();
  ui::WindowShowState window_state = native_app_window_->GetRestoredState();
  cache->SaveGeometry(
      extension_id(), window_key_, bounds, screen_bounds, window_state);
}

void AppWindow::AdjustBoundsToBeVisibleOnScreen(
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

AppWindow::CreateParams AppWindow::LoadDefaults(CreateParams params)
    const {
  // Ensure width and height are specified.
  if (params.content_spec.bounds.width() == 0 &&
      params.window_spec.bounds.width() == 0) {
    params.content_spec.bounds.set_width(kDefaultWidth);
  }
  if (params.content_spec.bounds.height() == 0 &&
      params.window_spec.bounds.height() == 0) {
    params.content_spec.bounds.set_height(kDefaultHeight);
  }

  // If left and top are left undefined, the native app window will center
  // the window on the main screen in a platform-defined manner.

  // Load cached state if it exists.
  if (!params.window_key.empty()) {
    AppWindowGeometryCache* cache =
        AppWindowGeometryCache::Get(browser_context());

    gfx::Rect cached_bounds;
    gfx::Rect cached_screen_bounds;
    ui::WindowShowState cached_state = ui::SHOW_STATE_DEFAULT;
    if (cache->GetGeometry(extension_id(),
                           params.window_key,
                           &cached_bounds,
                           &cached_screen_bounds,
                           &cached_state)) {
      // App window has cached screen bounds, make sure it fits on screen in
      // case the screen resolution changed.
      gfx::Screen* screen = gfx::Screen::GetNativeScreen();
      gfx::Display display = screen->GetDisplayMatching(cached_bounds);
      gfx::Rect current_screen_bounds = display.work_area();
      SizeConstraints constraints(params.GetWindowMinimumSize(gfx::Insets()),
                                  params.GetWindowMaximumSize(gfx::Insets()));
      AdjustBoundsToBeVisibleOnScreen(cached_bounds,
                                      cached_screen_bounds,
                                      current_screen_bounds,
                                      constraints.GetMinimumSize(),
                                      &params.window_spec.bounds);
      params.state = cached_state;

      // Since we are restoring a cached state, reset the content bounds spec to
      // ensure it is not used.
      params.content_spec.ResetBounds();
    }
  }

  return params;
}

// static
SkRegion* AppWindow::RawDraggableRegionsToSkRegion(
    const std::vector<extensions::DraggableRegion>& regions) {
  SkRegion* sk_region = new SkRegion;
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           regions.begin();
       iter != regions.end();
       ++iter) {
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
