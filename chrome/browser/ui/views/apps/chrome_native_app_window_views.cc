// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

#include <stddef.h>
#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;

namespace {

const int kMinPanelWidth = 100;
const int kMinPanelHeight = 100;
const int kDefaultPanelWidth = 200;
const int kDefaultPanelHeight = 300;

struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

const AcceleratorMapping kAppWindowAcceleratorMap[] = {
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW },
};

// These accelerators will only be available in kiosk mode. These allow the
// user to manually zoom app windows. This is only necessary in kiosk mode
// (in normal mode, the user can zoom via the screen magnifier).
// TODO(xiyuan): Write a test for kiosk accelerators.
const AcceleratorMapping kAppWindowKioskAppModeAcceleratorMap[] = {
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
};

void AddAcceleratorsFromMapping(const AcceleratorMapping mapping[],
                                size_t mapping_length,
                                std::map<ui::Accelerator, int>* accelerators) {
  for (size_t i = 0; i < mapping_length; ++i) {
    ui::Accelerator accelerator(mapping[i].keycode, mapping[i].modifiers);
    (*accelerators)[accelerator] = mapping[i].command_id;
  }
}

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  typedef std::map<ui::Accelerator, int> AcceleratorMap;
  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, accelerators, ());
  if (!chrome::IsRunningInForcedAppMode()) {
    if (accelerators.empty()) {
      AddAcceleratorsFromMapping(
          kAppWindowAcceleratorMap,
          arraysize(kAppWindowAcceleratorMap),
          &accelerators);
    }
    return accelerators;
  }

  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, app_mode_accelerators, ());
  if (app_mode_accelerators.empty()) {
    AddAcceleratorsFromMapping(
        kAppWindowAcceleratorMap,
        arraysize(kAppWindowAcceleratorMap),
        &app_mode_accelerators);
    AddAcceleratorsFromMapping(
        kAppWindowKioskAppModeAcceleratorMap,
        arraysize(kAppWindowKioskAppModeAcceleratorMap),
        &app_mode_accelerators);
  }
  return app_mode_accelerators;
}

}  // namespace

ChromeNativeAppWindowViews::ChromeNativeAppWindowViews()
    : has_frame_color_(false),
      active_frame_color_(SK_ColorBLACK),
      inactive_frame_color_(SK_ColorBLACK) {
}

ChromeNativeAppWindowViews::~ChromeNativeAppWindowViews() {}

void ChromeNativeAppWindowViews::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::OnBeforePanelWidgetInit(
    bool use_default_bounds,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::InitializeDefaultWindow(
    const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.remove_standard_frame = IsFrameless() || has_frame_color_;
  init_params.use_system_default_icon = true;
  if (create_params.alpha_enabled) {
    init_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (IsFrameless())
      init_params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  }
  init_params.keep_on_top = create_params.always_on_top;
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;

  OnBeforeWidgetInit(create_params, &init_params, widget());
  widget()->Init(init_params);

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();
  gfx::Rect window_bounds = create_params.GetInitialWindowBounds(frame_insets);
  SetContentSizeConstraints(create_params.GetContentMinimumSize(frame_insets),
                            create_params.GetContentMaximumSize(frame_insets));
  if (!window_bounds.IsEmpty()) {
    using BoundsSpecification = AppWindow::BoundsSpecification;
    bool position_specified =
        window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
        window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
    if (!position_specified)
      widget()->CenterWindow(window_bounds.size());
    else
      widget()->SetBounds(window_bounds);
  }

#if defined(OS_CHROMEOS)
  if (create_params.is_ime_window)
    return;
#endif

  // Register accelarators supported by app windows.
  // TODO(jeremya/stevenjb): should these be registered for panels too?
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  const bool is_kiosk_app_mode = chrome::IsRunningInForcedAppMode();

  // Ensures that kiosk mode accelerators are enabled when in kiosk mode (to be
  // future proof). This is needed because GetAcceleratorTable() uses a static
  // to store data and only checks kiosk mode once. If a platform app is
  // launched before kiosk mode starts, the kiosk accelerators will not be
  // registered. This CHECK catches the case.
  CHECK(!is_kiosk_app_mode ||
        accelerator_table.size() ==
            arraysize(kAppWindowAcceleratorMap) +
                arraysize(kAppWindowKioskAppModeAcceleratorMap));

  // Ensure there is a ZoomController in kiosk mode, otherwise the processing
  // of the accelerators will cause a crash. Note CHECK here because DCHECK
  // will not be noticed, as this could only be relevant on real hardware.
  CHECK(!is_kiosk_app_mode ||
        zoom::ZoomController::FromWebContents(web_view()->GetWebContents()));

  for (std::map<ui::Accelerator, int>::const_iterator iter =
           accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    if (is_kiosk_app_mode && !chrome::IsCommandAllowedInAppMode(iter->second))
      continue;

    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
}

void ChromeNativeAppWindowViews::InitializePanelWindow(
    const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.delegate = this;

  gfx::Rect initial_window_bounds =
      create_params.GetInitialWindowBounds(gfx::Insets());
  preferred_size_ = gfx::Size(initial_window_bounds.width(),
                              initial_window_bounds.height());
  if (preferred_size_.width() == 0)
    preferred_size_.set_width(kDefaultPanelWidth);
  else if (preferred_size_.width() < kMinPanelWidth)
    preferred_size_.set_width(kMinPanelWidth);

  if (preferred_size_.height() == 0)
    preferred_size_.set_height(kDefaultPanelHeight);
  else if (preferred_size_.height() < kMinPanelHeight)
    preferred_size_.set_height(kMinPanelHeight);

  // When a panel is not docked it will be placed at a default origin in the
  // currently active target root window.
  // TODO(afakhry): Remove Docked Windows in M58.
  bool use_default_bounds = create_params.state != ui::SHOW_STATE_DOCKED;
  // Sanitize initial origin reseting it in case it was not specified.
  using BoundsSpecification = AppWindow::BoundsSpecification;
  bool position_specified =
      initial_window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
      initial_window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
  params.bounds = (use_default_bounds || !position_specified) ?
      gfx::Rect(preferred_size_) :
      gfx::Rect(initial_window_bounds.origin(), preferred_size_);
  OnBeforePanelWidgetInit(use_default_bounds, &params, widget());
  widget()->Init(params);
  widget()->set_focus_on_creation(create_params.focused);
}

views::NonClientFrameView*
ChromeNativeAppWindowViews::CreateStandardDesktopAppFrame() {
  return views::WidgetDelegateView::CreateNonClientFrameView(widget());
}

// ui::BaseWindow implementation.

gfx::Rect ChromeNativeAppWindowViews::GetRestoredBounds() const {
  return widget()->GetRestoredBounds();
}

ui::WindowShowState ChromeNativeAppWindowViews::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_NORMAL;
}

bool ChromeNativeAppWindowViews::IsAlwaysOnTop() const {
  // TODO(jackhou): On Mac, only docked panels are always-on-top.
  return app_window()->window_type_is_panel() || widget()->IsAlwaysOnTop();
}

// views::WidgetDelegate implementation.

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowAppIcon() {
  gfx::Image app_icon = app_window()->app_icon();
  if (app_icon.IsEmpty())
    return GetWindowIcon();
  else
    return *app_icon.ToImageSkia();
}

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowIcon() {
  content::WebContents* web_contents = app_window()->web_contents();
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    gfx::Image app_icon = favicon_driver->GetFavicon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

views::NonClientFrameView* ChromeNativeAppWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return (IsFrameless() || has_frame_color_) ?
      CreateNonStandardAppFrame() : CreateStandardDesktopAppFrame();
}

bool ChromeNativeAppWindowViews::WidgetHasHitTestMask() const {
  return shape_ != NULL;
}

void ChromeNativeAppWindowViews::GetWidgetHitTestMask(gfx::Path* mask) const {
  shape_->getBoundaryPath(mask);
}

// views::View implementation.

gfx::Size ChromeNativeAppWindowViews::GetPreferredSize() const {
  if (!preferred_size_.IsEmpty())
    return preferred_size_;
  return NativeAppWindowViews::GetPreferredSize();
}

bool ChromeNativeAppWindowViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table.find(accelerator);
  DCHECK(iter != accelerator_table.end());
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_WINDOW:
      Close();
      return true;
    case IDC_ZOOM_MINUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_OUT);
      return true;
    case IDC_ZOOM_NORMAL:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_RESET);
      return true;
    case IDC_ZOOM_PLUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(), content::PAGE_ZOOM_IN);
      return true;
    default:
      NOTREACHED() << "Unknown accelerator sent to app window.";
  }
  return NativeAppWindowViews::AcceleratorPressed(accelerator);
}

// NativeAppWindow implementation.

void ChromeNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  // Fullscreen not supported by panels.
  if (app_window()->window_type_is_panel())
    return;

  widget()->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool ChromeNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void ChromeNativeAppWindowViews::UpdateShape(std::unique_ptr<SkRegion> region) {
  shape_ = std::move(region);
  widget()->SetShape(shape() ? base::MakeUnique<SkRegion>(*shape()) : nullptr);
  widget()->OnSizeConstraintsChanged();
}

bool ChromeNativeAppWindowViews::HasFrameColor() const {
  return has_frame_color_;
}

SkColor ChromeNativeAppWindowViews::ActiveFrameColor() const {
  return active_frame_color_;
}

SkColor ChromeNativeAppWindowViews::InactiveFrameColor() const {
  return inactive_frame_color_;
}

// NativeAppWindowViews implementation.

void ChromeNativeAppWindowViews::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  DCHECK(widget());
  has_frame_color_ = create_params.has_frame_color;
  active_frame_color_ = create_params.active_frame_color;
  inactive_frame_color_ = create_params.inactive_frame_color;
  if (create_params.window_type == AppWindow::WINDOW_TYPE_PANEL ||
      create_params.window_type == AppWindow::WINDOW_TYPE_V1_PANEL) {
    InitializePanelWindow(create_params);
  } else {
    InitializeDefaultWindow(create_params);
  }
  extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
      Profile::FromBrowserContext(app_window->browser_context()),
      widget()->GetFocusManager(),
      extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
      NULL));
}
