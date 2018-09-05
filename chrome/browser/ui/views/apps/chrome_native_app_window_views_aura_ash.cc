// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/frame/non_client_frame_view_ash.h"  // mash-ok
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/wm/window_state.h"  // mash-ok
#include "base/logging.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using extensions::AppWindow;

ChromeNativeAppWindowViewsAuraAsh::ChromeNativeAppWindowViewsAuraAsh()
    : exclusive_access_manager_(
          std::make_unique<ExclusiveAccessManager>(this)) {
  if (TabletModeClient::Get())
    TabletModeClient::Get()->AddObserver(this);
}

ChromeNativeAppWindowViewsAuraAsh::~ChromeNativeAppWindowViewsAuraAsh() {
  if (TabletModeClient::Get())
    TabletModeClient::Get()->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// NativeAppWindowViews implementation:
void ChromeNativeAppWindowViewsAuraAsh::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  ChromeNativeAppWindowViewsAura::InitializeWindow(app_window, create_params);
  aura::Window* window = GetNativeWindow();
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CHROME_APP));
  // Fullscreen doesn't always imply immersive mode (see
  // ShouldEnableImmersive()).
  window->SetProperty(ash::kImmersiveImpliedByFullscreen, false);

  observed_window_.Add(
      features::IsUsingWindowService() ? window->GetRootWindow() : window);
}

///////////////////////////////////////////////////////////////////////////////
// ChromeNativeAppWindowViews implementation:
void ChromeNativeAppWindowViewsAuraAsh::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(create_params, init_params,
                                                     widget);
  if (create_params.is_ime_window || create_params.show_on_lock_screen) {
    // Put ime windows and lock screen windows into their respective window
    // containers on the primary display.
    int container_id = create_params.is_ime_window
                           ? ash::kShellWindowId_ImeWindowParentContainer
                           : ash::kShellWindowId_LockActionHandlerContainer;
    ash_util::SetupWidgetInitParamsForContainer(init_params, container_id);
  }
  init_params->mus_properties
      [ws::mojom::WindowManager::kRemoveStandardFrame_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(init_params->remove_standard_frame);
  if (HasFrameColor()) {
    init_params
        ->mus_properties[ws::mojom::WindowManager::kFrameActiveColor_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int64_t>(ActiveFrameColor()));
    init_params->mus_properties
        [ws::mojom::WindowManager::kFrameInactiveColor_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int64_t>(InactiveFrameColor()));
  }
  init_params
      ->mus_properties[ws::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_APP));
  init_params
      ->mus_properties[ws::mojom::WindowManager::kWindowTitleShown_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int64_t>(false));
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsAuraAsh::CreateNonStandardAppFrame() {
  apps::AppWindowFrameView* frame = new apps::AppWindowFrameView(widget(), this,
      HasFrameColor(), ActiveFrameColor(), InactiveFrameColor());
  frame->Init();

  // For Aura windows on the Ash desktop the sizes are different and the user
  // can resize the window from slightly outside the bounds as well.
  frame->SetResizeSizes(ash::kResizeInsideBoundsSize,
                        ash::kResizeOutsideBoundsSize,
                        ash::kResizeAreaCornerSize);
  return frame;
}

bool ChromeNativeAppWindowViewsAuraAsh::ShouldRemoveStandardFrame() {
  if (IsFrameless())
    return true;

  return HasFrameColor() && !features::IsUsingWindowService();
}

///////////////////////////////////////////////////////////////////////////////
// ui::BaseWindow implementation:
gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetRestoredBounds() const {
  gfx::Rect* bounds =
      GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds && !bounds->IsEmpty())
    return *bounds;

  return ChromeNativeAppWindowViewsAura::GetRestoredBounds();
}

ui::WindowShowState
ChromeNativeAppWindowViewsAuraAsh::GetRestoredState() const {
  // Use kPreMinimizedShowStateKey in case a window is minimized/hidden.
  ui::WindowShowState restore_state =
      GetNativeWindow()->GetProperty(aura::client::kPreMinimizedShowStateKey);

  bool is_fullscreen = false;
  if (GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey)) {
    // If an override is given, use that restore state, unless the window is in
    // immersive fullscreen.
    restore_state = ash::ToWindowShowState(GetNativeWindow()->GetProperty(
        ash::kRestoreWindowStateTypeOverrideKey));
    is_fullscreen = restore_state == ui::SHOW_STATE_FULLSCREEN;
  } else {
    if (IsMaximized())
      return ui::SHOW_STATE_MAXIMIZED;
    is_fullscreen = IsFullscreen();
  }

  if (is_fullscreen) {
    if (IsImmersiveModeEnabled()) {
      // Restore windows which were previously in immersive fullscreen to
      // maximized. Restoring the window to a different fullscreen type
      // makes for a bad experience.
      return ui::SHOW_STATE_MAXIMIZED;
    }
    return ui::SHOW_STATE_FULLSCREEN;
  }

  return GetRestorableState(restore_state);
}

bool ChromeNativeAppWindowViewsAuraAsh::IsAlwaysOnTop() const {
  return widget()->IsAlwaysOnTop();
}

///////////////////////////////////////////////////////////////////////////////
// views::ContextMenuController implementation:
void ChromeNativeAppWindowViewsAuraAsh::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  menu_model_ = CreateMultiUserContextMenu(GetNativeWindow());
  if (!menu_model_.get())
    return;

  // Only show context menu if point is in caption.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(widget()->non_client_view(),
                                      &point_in_view_coords);
  int hit_test =
      widget()->non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        base::Bind(&ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed,
                   base::Unretained(this)));
    menu_runner_->RunMenuAt(source->GetWidget(), NULL,
                            gfx::Rect(p, gfx::Size(0, 0)),
                            views::MENU_ANCHOR_TOPLEFT, source_type);
  } else {
    menu_model_.reset();
  }
}

///////////////////////////////////////////////////////////////////////////////
// WidgetDelegate implementation:
views::NonClientFrameView*
ChromeNativeAppWindowViewsAuraAsh::CreateNonClientFrameView(
    views::Widget* widget) {
  if (IsFrameless())
    return CreateNonStandardAppFrame();

  if (features::IsUsingWindowService())
    return nullptr;

  observed_window_state_.Add(ash::wm::GetWindowState(GetNativeWindow()));

  ash::NonClientFrameViewAsh* custom_frame_view =
      new ash::NonClientFrameViewAsh(widget);

  custom_frame_view->GetHeaderView()->set_context_menu_controller(this);

  // Enter immersive mode if the app is opened in tablet mode with the hide
  // titlebars feature enabled.
  UpdateImmersiveMode();

  if (HasFrameColor()) {
    custom_frame_view->SetFrameColors(ActiveFrameColor(),
                                      InactiveFrameColor());
  }

  return custom_frame_view;
}

///////////////////////////////////////////////////////////////////////////////
// NativeAppWindow implementation:
void ChromeNativeAppWindowViewsAuraAsh::SetFullscreen(int fullscreen_types) {
  ChromeNativeAppWindowViewsAura::SetFullscreen(fullscreen_types);
  UpdateImmersiveMode();

  // In a public session, display a toast with instructions on exiting
  // fullscreen.
  if (profiles::IsPublicSession()) {
    UpdateExclusiveAccessExitBubbleContent(
        GURL(),
        fullscreen_types & (AppWindow::FULLSCREEN_TYPE_HTML_API |
                            AppWindow::FULLSCREEN_TYPE_WINDOW_API)
            ? EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION
            : EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
        ExclusiveAccessBubbleHideCallback(),
        /*force_update=*/false);
  }

  // Autohide the shelf instead of hiding the shelf completely when only in
  // OS fullscreen or when in a public session.
  const bool should_hide_shelf =
      !profiles::IsPublicSession() &&
      fullscreen_types != AppWindow::FULLSCREEN_TYPE_OS;
  widget()->GetNativeWindow()->SetProperty(ash::kHideShelfWhenFullscreenKey,
                                           should_hide_shelf);
  widget()->non_client_view()->Layout();
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  ChromeNativeAppWindowViewsAura::UpdateDraggableRegions(regions);

  SkRegion* draggable_region = GetDraggableRegion();
  // Set the NativeAppWindow's draggable region on the mus window.
  if (draggable_region && !draggable_region->isEmpty() && widget() &&
      features::IsUsingWindowService()) {
    // Supply client area insets that encompass all draggable regions.
    gfx::Insets insets(draggable_region->getBounds().bottom(), 0, 0, 0);

    // Invert the draggable regions to determine the additional client areas.
    SkRegion inverted_region;
    inverted_region.setRect(draggable_region->getBounds());
    inverted_region.op(*draggable_region, SkRegion::kDifference_Op);
    std::vector<gfx::Rect> additional_client_regions;
    for (SkRegion::Iterator i(inverted_region); !i.done(); i.next())
      additional_client_regions.push_back(gfx::SkIRectToRect(i.rect()));

    aura::WindowTreeHostMus* window_tree_host =
        static_cast<aura::WindowTreeHostMus*>(
            widget()->GetNativeWindow()->GetHost());
    window_tree_host->SetClientArea(insets,
                                    std::move(additional_client_regions));
  }
}

void ChromeNativeAppWindowViewsAuraAsh::SetActivateOnPointer(
    bool activate_on_pointer) {
  widget()->GetNativeWindow()->SetProperty(aura::client::kActivateOnPointerKey,
                                           activate_on_pointer);
}

///////////////////////////////////////////////////////////////////////////////
// TabletModeClientObserver implementation:
void ChromeNativeAppWindowViewsAuraAsh::OnTabletModeToggled(bool enabled) {
  tablet_mode_enabled_ = enabled;
  UpdateImmersiveMode();
  widget()->non_client_view()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// ui::AcceleratorProvider implementation:
bool ChromeNativeAppWindowViewsAuraAsh::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // Normally |accelerator| is used to determine the text in the bubble;
  // however, for the fullscreen type set in SetFullscreen(), the bubble
  // currently ignores it, and will always use IDS_APP_ESC_KEY. Be explicit here
  // anyway.
  *accelerator = ui::Accelerator(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExclusiveAccessContext implementation:
Profile* ChromeNativeAppWindowViewsAuraAsh::GetProfile() {
  return Profile::FromBrowserContext(web_view()->browser_context());
}

bool ChromeNativeAppWindowViewsAuraAsh::IsFullscreen() const {
  return NativeAppWindowViews::IsFullscreen();
}

void ChromeNativeAppWindowViewsAuraAsh::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  // This codepath is never hit for Chrome Apps.
  NOTREACHED();
}

void ChromeNativeAppWindowViewsAuraAsh::ExitFullscreen() {
  // This codepath is never hit for Chrome Apps.
  NOTREACHED();
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {
    exclusive_access_bubble_.reset();
    if (bubble_first_hide_callback) {
      std::move(bubble_first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }
    return;
  }

  if (exclusive_access_bubble_) {
    exclusive_access_bubble_->UpdateContent(
        url, bubble_type, std::move(bubble_first_hide_callback), force_update);
    return;
  }

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleViews>(
      this, url, bubble_type, std::move(bubble_first_hide_callback));
}

void ChromeNativeAppWindowViewsAuraAsh::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_)
    exclusive_access_bubble_->OnUserInput();
}

content::WebContents*
ChromeNativeAppWindowViewsAuraAsh::GetActiveWebContents() {
  return web_view()->web_contents();
}

void ChromeNativeAppWindowViewsAuraAsh::UnhideDownloadShelf() {}

void ChromeNativeAppWindowViewsAuraAsh::HideDownloadShelf() {}

bool ChromeNativeAppWindowViewsAuraAsh::ShouldHideUIForFullscreen() const {
  return false;
}

ExclusiveAccessBubbleViews*
ChromeNativeAppWindowViewsAuraAsh::GetExclusiveAccessBubble() {
  return exclusive_access_bubble_.get();
}

///////////////////////////////////////////////////////////////////////////////
// ExclusiveAccessBubbleViewsContext implementation:
ExclusiveAccessManager*
ChromeNativeAppWindowViewsAuraAsh::GetExclusiveAccessManager() {
  return exclusive_access_manager_.get();
}

views::Widget* ChromeNativeAppWindowViewsAuraAsh::GetBubbleAssociatedWidget() {
  return widget();
}

ui::AcceleratorProvider*
ChromeNativeAppWindowViewsAuraAsh::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView ChromeNativeAppWindowViewsAuraAsh::GetBubbleParentView() const {
  return widget()->GetNativeView();
}

gfx::Point ChromeNativeAppWindowViewsAuraAsh::GetCursorPointInParent() const {
  gfx::Point cursor_pos = display::Screen::GetScreen()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(widget()->GetRootView(), &cursor_pos);
  return cursor_pos;
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetClientAreaBoundsInScreen()
    const {
  return widget()->GetClientAreaBoundsInScreen();
}

bool ChromeNativeAppWindowViewsAuraAsh::IsImmersiveModeEnabled() const {
  return GetWidget()->GetNativeWindow()->GetProperty(ash::kImmersiveIsActive);
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetTopContainerBoundsInScreen() {
  gfx::Rect* bounds = GetWidget()->GetNativeWindow()->GetProperty(
      ash::kImmersiveTopContainerBoundsInScreen);
  return bounds ? *bounds : gfx::Rect();
}

void ChromeNativeAppWindowViewsAuraAsh::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::CanTriggerOnMouse() const {
  return true;
}

void ChromeNativeAppWindowViewsAuraAsh::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  ChromeNativeAppWindowViewsAura::OnWidgetActivationChanged(widget, active);
  // In splitview, minimized windows go back into the overview grid. If we
  // minimize by using the minimize button on the immersive header, the
  // overview window will calculate the title bar offset and the window will be
  // missing its top portion. Prevent this by disabling immersive mode upon
  // minimize.
  UpdateImmersiveMode();
}

void ChromeNativeAppWindowViewsAuraAsh::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  DCHECK(!features::IsUsingWindowService());
  DCHECK_EQ(GetNativeWindow(), window_state->window());
  if (window_state->IsFullscreen() != app_window()->IsFullscreen()) {
    // Report OS-initiated state changes to |app_window()|. This is done in
    // OnPostWindowStateTypeChange rather than OnWindowPropertyChanged because
    // WindowState saves restore bounds *after* changing the property, and
    // enabling immersive mode will change the current bounds before the old
    // bounds can be saved.
    if (window_state->IsFullscreen())
      app_window()->OSFullscreen();
    else
      app_window()->OnNativeWindowChanged();
  }
}

void ChromeNativeAppWindowViewsAuraAsh::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;

  auto new_state = window->GetProperty(aura::client::kShowStateKey);

  if (new_state != ui::SHOW_STATE_FULLSCREEN &&
      new_state != ui::SHOW_STATE_MINIMIZED && app_window()->IsFullscreen()) {
    app_window()->Restore();
  } else if (features::IsUsingWindowService() &&
             new_state == ui::SHOW_STATE_FULLSCREEN &&
             !app_window()->IsFullscreen()) {
    app_window()->OSFullscreen();
  }

  // Usually OnNativeWindowChanged() is called when the window bounds are
  // changed as a result of a state type change. Because the change in
  // state type has already occurred, we need to call
  // OnNativeWindowChanged() explicitly.
  app_window()->OnNativeWindowChanged();
  UpdateImmersiveMode();
}

void ChromeNativeAppWindowViewsAuraAsh::OnWindowDestroying(
    aura::Window* window) {
  if (observed_window_state_.IsObservingSources())
    observed_window_state_.Remove(ash::wm::GetWindowState(window));
  observed_window_.Remove(window);
}

void ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::ShouldEnableImmersiveMode() const {
  // No immersive mode for forced fullscreen.
  if (app_window()->IsForcedFullscreen())
    return false;

  // Always use immersive mode in a public session in fullscreen state.
  if (profiles::IsPublicSession() && IsFullscreen())
    return true;

  // Always use immersive mode when fullscreen is set by the OS.
  if (app_window()->IsOsFullscreen())
    return true;

  TabletModeClient* client = TabletModeClient::Get();
  // Windows in tablet mode which are resizable have their title bars
  // hidden in ash for more size, so enable immersive mode so users
  // have access to window controls. Non resizable windows do not gain
  // size by hidding the title bar, so it is not hidden and thus there
  // is no need for immersive mode.
  // TODO(sammiequon): Investigate whether we should check
  // resizability using WindowState instead of CanResize.
  // TODO(crbug.com/801619): This adds a little extra animation
  // when minimizing or unminimizing window.
  return client && client->tablet_mode_enabled() && CanResize() &&
         !IsMinimized();
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateImmersiveMode() {
  ash::ImmersiveFullscreenController::EnableForWidget(
      widget(), ShouldEnableImmersiveMode());
}
