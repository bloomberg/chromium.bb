// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/ash_constants.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/panels/panel_frame_view.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using extensions::AppWindow;

namespace {

// This class handles a user's fullscreen request (Shift+F4/F4).
class NativeAppWindowStateDelegate : public ash::wm::WindowStateDelegate,
                                     public aura::WindowObserver {
 public:
  NativeAppWindowStateDelegate(AppWindow* app_window,
                               extensions::NativeAppWindow* native_app_window)
      : app_window_(app_window),
        window_state_(
            ash::wm::GetWindowState(native_app_window->GetNativeWindow())) {
    // Add a window state observer to exit fullscreen properly in case
    // fullscreen is exited without going through AppWindow::Restore(). This
    // is the case when exiting immersive fullscreen via the "Restore" window
    // control.
    // TODO(pkotwicz): This is a hack. Remove ASAP. http://crbug.com/319048
    window_state_->window()->AddObserver(this);
  }
  ~NativeAppWindowStateDelegate() override {
    if (window_state_)
      window_state_->window()->RemoveObserver(this);
  }

 private:
  // Overridden from ash::wm::WindowStateDelegate.
  bool ToggleFullscreen(ash::wm::WindowState* window_state) override {
    // Windows which cannot be maximized should not be fullscreened.
    DCHECK(window_state->IsFullscreen() || window_state->CanMaximize());
    if (window_state->IsFullscreen())
      app_window_->Restore();
    else if (window_state->CanMaximize())
      app_window_->OSFullscreen();
    return true;
  }

  // Overridden from ash::wm::WindowStateDelegate.
  bool RestoreAlwaysOnTop(ash::wm::WindowState* window_state) override {
    app_window_->RestoreAlwaysOnTop();
    return true;
  }

  // Overridden from aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == ash::kWindowStateTypeKey) {
      ash::mojom::WindowStateType new_state =
          window->GetProperty(ash::kWindowStateTypeKey);

      if (new_state != ash::mojom::WindowStateType::FULLSCREEN &&
          new_state != ash::mojom::WindowStateType::MINIMIZED &&
          app_window_->GetBaseWindow() &&
          app_window_->GetBaseWindow()->IsFullscreenOrPending()) {
        app_window_->Restore();
        // Usually OnNativeWindowChanged() is called when the window bounds are
        // changed as a result of a state type change. Because the change in
        // state type has already occurred, we need to call
        // OnNativeWindowChanged() explicitly.
        app_window_->OnNativeWindowChanged();
      }
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    window_state_->window()->RemoveObserver(this);
    window_state_ = nullptr;
  }

  // Not owned.
  AppWindow* app_window_;
  ash::wm::WindowState* window_state_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowStateDelegate);
};

}  // namespace

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
  aura::Window* window = widget()->GetNativeWindow();

  if (!app_window->window_type_is_panel()) {
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::CHROME_APP));
  }
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
    if (ash_util::IsRunningInMash()) {
      init_params->mus_properties
          [ui::mojom::WindowManager::kContainerId_InitProperty] =
          mojo::ConvertTo<std::vector<uint8_t>>(container_id);
      int display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
      init_params
          ->mus_properties[ui::mojom::WindowManager::kDisplayId_InitProperty] =
          mojo::ConvertTo<std::vector<uint8_t>>(display_id);
    } else {
      init_params->parent = ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(), container_id);
    }
  }
  DCHECK_NE(AppWindow::WINDOW_TYPE_PANEL, create_params.window_type);
  init_params->mus_properties
      [ui::mojom::WindowManager::kRemoveStandardFrame_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(init_params->remove_standard_frame);
  init_params
      ->mus_properties[ui::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_APP));
}

void ChromeNativeAppWindowViewsAuraAsh::OnBeforePanelWidgetInit(
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViewsAura::OnBeforePanelWidgetInit(init_params, widget);

  if (!ash_util::IsRunningInMash() && ash::Shell::HasInstance()) {
    // Open a new panel on the target root.
    init_params->context = ash::Shell::GetRootWindowForNewWindows();
    init_params->bounds = gfx::Rect(GetPreferredSize());
    wm::ConvertRectToScreen(ash::Shell::GetRootWindowForNewWindows(),
                            &init_params->bounds);
  }
  init_params
      ->mus_properties[ui::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_APP_PANEL));
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

///////////////////////////////////////////////////////////////////////////////
// ui::BaseWindow implementation:
gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetRestoredBounds() const {
  gfx::Rect* bounds =
      widget()->GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds && !bounds->IsEmpty())
    return *bounds;

  return ChromeNativeAppWindowViewsAura::GetRestoredBounds();
}

ui::WindowShowState
ChromeNativeAppWindowViewsAuraAsh::GetRestoredState() const {
  // Use kPreMinimizedShowStateKey in case a window is minimized/hidden.
  ui::WindowShowState restore_state = widget()->GetNativeWindow()->GetProperty(
      aura::client::kPreMinimizedShowStateKey);

  bool is_fullscreen = false;
  if (widget()->GetNativeWindow()->GetProperty(
          ash::kRestoreBoundsOverrideKey)) {
    // If an override is given, use that restore state, unless the window is in
    // immersive fullscreen.
    restore_state = widget()->GetNativeWindow()->GetProperty(
        ash::kRestoreShowStateOverrideKey);
    is_fullscreen = restore_state == ui::SHOW_STATE_FULLSCREEN;
  } else {
    if (IsMaximized())
      return ui::SHOW_STATE_MAXIMIZED;
    is_fullscreen = IsFullscreen();
  }

  if (is_fullscreen) {
    if (immersive_fullscreen_controller_.get() &&
        immersive_fullscreen_controller_->IsEnabled()) {
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
  if (app_window()->window_type_is_panel())
    return widget()->GetNativeWindow()->GetProperty(ash::kPanelAttachedKey);
  return widget()->IsAlwaysOnTop();
}

///////////////////////////////////////////////////////////////////////////////
// views::ContextMenuController implementation:
void ChromeNativeAppWindowViewsAuraAsh::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  menu_model_ = CreateMultiUserContextMenu(app_window()->GetNativeWindow());
  if (!menu_model_.get())
    return;

  // Only show context menu if point is in caption.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(widget()->non_client_view(),
                                      &point_in_view_coords);
  int hit_test =
      widget()->non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION) {
    menu_model_adapter_.reset(new views::MenuModelAdapter(
        menu_model_.get(),
        base::Bind(&ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed,
                   base::Unretained(this))));
    menu_runner_.reset(new views::MenuRunner(
        menu_model_adapter_->CreateMenu(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
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
  // Set the delegate now because CustomFrameViewAsh sets the
  // WindowStateDelegate if one is not already set.
  ash::wm::GetWindowState(GetNativeWindow())
      ->SetDelegate(std::unique_ptr<ash::wm::WindowStateDelegate>(
          new NativeAppWindowStateDelegate(app_window(), this)));

  if (IsFrameless())
    return CreateNonStandardAppFrame();

  if (ash_util::IsRunningInMash())
    return ChromeNativeAppWindowViews::CreateNonClientFrameView(widget);

  if (app_window()->window_type_is_panel()) {
    ash::PanelFrameView* frame_view =
        new ash::PanelFrameView(widget, ash::PanelFrameView::FRAME_ASH);
    frame_view->set_context_menu_controller(this);
    if (HasFrameColor())
      frame_view->SetFrameColors(ActiveFrameColor(), InactiveFrameColor());
    return frame_view;
  }

  ash::CustomFrameViewAsh* custom_frame_view =
      new ash::CustomFrameViewAsh(widget);
  // Non-frameless app windows can be put into immersive fullscreen.
  immersive_fullscreen_controller_.reset(
      new ash::ImmersiveFullscreenController());
  custom_frame_view->InitImmersiveFullscreenControllerForView(
      immersive_fullscreen_controller_.get());
  custom_frame_view->GetHeaderView()->set_context_menu_controller(this);

  // Enter immersive mode if the app is opened in tablet mode with the hide
  // titlebars feature enabled.
  if (!ash_util::IsRunningInMash()) {
    DCHECK(ash::Shell::HasInstance());
    if (CanAutohideTitlebarsInTabletMode()) {
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_PACKAGED_APP, true);
    }
  }

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

  if (immersive_fullscreen_controller_.get()) {
    // Immersive mode should not change if we set fullscreen on a maximizable
    // app in tablet mode when the hide titlebars feature is enabled.
    bool autohide_titlebars_enabled = false;
    if (!ash_util::IsRunningInMash())
      autohide_titlebars_enabled = CanAutohideTitlebarsInTabletMode();

    if (!autohide_titlebars_enabled) {
      // |immersive_fullscreen_controller_| should only be set if immersive
      // fullscreen is the fullscreen type used by the OS, or if we're in a
      // public session where we always use immersive.
      const bool immersive_enabled =
          profiles::IsPublicSession() ||
          (fullscreen_types & AppWindow::FULLSCREEN_TYPE_OS) != 0;
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_PACKAGED_APP,
          immersive_enabled);

      // In a public session, display a toast with instructions on exiting
      // fullscreen.
      if (profiles::IsPublicSession()) {
        UpdateExclusiveAccessExitBubbleContent(
            GURL(),
            fullscreen_types & (AppWindow::FULLSCREEN_TYPE_HTML_API |
                                AppWindow::FULLSCREEN_TYPE_WINDOW_API)
                ? EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION
                : EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            ExclusiveAccessBubbleHideCallback());
      }
    }

    // Autohide the shelf instead of hiding the shelf completely when only in
    // OS fullscreen or when in a public session.
    const bool should_hide_shelf = !profiles::IsPublicSession() &&
        fullscreen_types != AppWindow::FULLSCREEN_TYPE_OS;
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(widget()->GetNativeWindow());
    window_state->set_hide_shelf_when_fullscreen(should_hide_shelf);
    if (!ash_util::IsRunningInMash()) {
      DCHECK(ash::Shell::HasInstance());
      ash::Shell::Get()->UpdateShelfVisibility();
    }
  }
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  ChromeNativeAppWindowViewsAura::UpdateDraggableRegions(regions);

  SkRegion* draggable_region = GetDraggableRegion();
  // Set the NativeAppWindow's draggable region on the mus window.
  if (draggable_region && !draggable_region->isEmpty() && widget() &&
      aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS) {
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

  if (!immersive_fullscreen_controller_)
    return;

  if (enabled) {
    // Enter immersive mode if the widget can maximize and the hide titlebars
    // in tablet mode feature is enabled.
    if (CanAutohideTitlebarsInTabletMode()) {
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_PACKAGED_APP, true);
    }
  } else {
    // Exit immersive mode if the widget is not in fullscreen and can maximize.
    if (!widget()->IsFullscreen() && CanMaximize()) {
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_PACKAGED_APP, false);
    }
  }
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
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback) {
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
        url, bubble_type, std::move(bubble_first_hide_callback));
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
  return immersive_fullscreen_controller_->IsEnabled();
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetTopContainerBoundsInScreen() {
  return immersive_fullscreen_controller_->top_container()->GetBoundsInScreen();
}

void ChromeNativeAppWindowViewsAuraAsh::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::CanTriggerOnMouse() const {
  return true;
}

void ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  menu_model_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::CanAutohideTitlebarsInTabletMode()
    const {
  TabletModeClient* client = TabletModeClient::Get();
  return CanMaximize() && client && client->tablet_mode_enabled() &&
         client->auto_hide_title_bars();
}
