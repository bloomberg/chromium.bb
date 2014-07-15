// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/native_widget.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/shell_integration_linux.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/session/session_state_delegate.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/ui/views/frame/browser_command_handler_x11.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : native_browser_frame_(NULL),
      root_view_(NULL),
      browser_frame_view_(NULL),
      browser_view_(browser_view),
      theme_provider_(ThemeServiceFactory::GetForProfile(
                          browser_view_->browser()->profile())) {
  browser_view_->set_frame(this);
  set_is_secondary_widget(false);
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);

#if defined(USE_X11)
  browser_command_handler_.reset(
      new BrowserCommandHandlerX11(browser_view_->browser()));
#endif
}

BrowserFrame::~BrowserFrame() {
  if (browser_command_handler_ && GetNativeView())
    GetNativeView()->RemovePreTargetHandler(browser_command_handler_.get());
}

// static
const gfx::FontList& BrowserFrame::GetTitleFontList() {
  static const gfx::FontList* title_font_list = new gfx::FontList();
  ANNOTATE_LEAKING_OBJECT_PTR(title_font_list);
  return *title_font_list;
}

void BrowserFrame::InitBrowserFrame() {
  use_custom_frame_pref_.Init(
      prefs::kUseCustomChromeFrame,
      browser_view_->browser()->profile()->GetPrefs(),
      base::Bind(&BrowserFrame::OnUseCustomChromeFrameChanged,
                 base::Unretained(this)));

  native_browser_frame_ =
      NativeBrowserFrameFactory::CreateNativeBrowserFrame(this, browser_view_);
  views::Widget::InitParams params;
  params.delegate = browser_view_;
  params.native_widget = native_browser_frame_->AsNativeWidget();
  if (browser_view_->browser()->is_type_tabbed()) {
    // Typed panel/popup can only return a size once the widget has been
    // created.
    chrome::GetSavedWindowBoundsAndShowState(browser_view_->browser(),
                                             &params.bounds,
                                             &params.show_state);
  }

  if (browser_view_->browser()->host_desktop_type() ==
      chrome::HOST_DESKTOP_TYPE_ASH || chrome::ShouldOpenAshOnStartup()) {
    params.context = ash::Shell::GetPrimaryRootWindow();
#if defined(OS_WIN)
    // If this window is under ASH on Windows, we need it to be translucent.
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
#endif
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Set up a custom WM_CLASS for some sorts of window types. This allows
  // task switchers in X11 environments to distinguish between main browser
  // windows and e.g app windows.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const Browser& browser = *browser_view_->browser();
  params.wm_class_class = shell_integration_linux::GetProgramClassName();
  params.wm_class_name = params.wm_class_class;
  if (browser.is_app() && !browser.is_devtools()) {
    // This window is a hosted app or v1 packaged app.
    // NOTE: v2 packaged app windows are created by ChromeNativeAppWindowViews.
    params.wm_class_name = web_app::GetWMClassFromAppName(browser.app_name());
  } else if (command_line.HasSwitch(switches::kUserDataDir)) {
    // Set the class name to e.g. "Chrome (/tmp/my-user-data)".  The
    // class name will show up in the alt-tab list in gnome-shell if
    // you're running a binary that doesn't have a matching .desktop
    // file.
    const std::string user_data_dir =
        command_line.GetSwitchValueNative(switches::kUserDataDir);
    params.wm_class_name += " (" + user_data_dir + ")";
  }
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  params.wm_role_name = browser_view_->browser()->is_type_tabbed() ?
      std::string(kX11WindowRoleBrowser) : std::string(kX11WindowRolePopup);

  params.remove_standard_frame = UseCustomFrame();
  set_frame_type(UseCustomFrame() ? Widget::FRAME_TYPE_FORCE_CUSTOM
                                  : Widget::FRAME_TYPE_FORCE_NATIVE);
#endif  // defined(OS_LINUX)

  Init(params);

  if (!native_browser_frame_->UsesNativeSystemMenu()) {
    DCHECK(non_client_view());
    non_client_view()->set_context_menu_controller(this);
  }

  if (browser_command_handler_)
    GetNativeWindow()->AddPreTargetHandler(browser_command_handler_.get());
}

void BrowserFrame::SetThemeProvider(scoped_ptr<ui::ThemeProvider> provider) {
  owned_theme_provider_ = provider.Pass();
  theme_provider_ = owned_theme_provider_.get();
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStrip(views::View* tabstrip) const {
  // This can be invoked before |browser_frame_view_| has been set.
  return browser_frame_view_ ?
      browser_frame_view_->GetBoundsForTabStrip(tabstrip) : gfx::Rect();
}

int BrowserFrame::GetTopInset() const {
  return browser_frame_view_->GetTopInset();
}

int BrowserFrame::GetThemeBackgroundXInset() const {
  return browser_frame_view_->GetThemeBackgroundXInset();
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

views::View* BrowserFrame::GetFrameView() const {
  return browser_frame_view_;
}

bool BrowserFrame::UseCustomFrame() const {
  return use_custom_frame_pref_.GetValue() &&
      browser_view_->IsBrowserTypeNormal();
}

bool BrowserFrame::ShouldSaveWindowPlacement() const {
  return native_browser_frame_->ShouldSaveWindowPlacement();
}

void BrowserFrame::GetWindowPlacement(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  return native_browser_frame_->GetWindowPlacement(bounds, show_state);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::Widget overrides:

views::internal::RootView* BrowserFrame::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

views::NonClientFrameView* BrowserFrame::CreateNonClientFrameView() {
  browser_frame_view_ =
      chrome::CreateBrowserNonClientFrameView(this, browser_view_);
  return browser_frame_view_;
}

bool BrowserFrame::GetAccelerator(int command_id,
                                  ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

ui::ThemeProvider* BrowserFrame::GetThemeProvider() const {
  return theme_provider_;
}

void BrowserFrame::SchedulePaintInRect(const gfx::Rect& rect) {
  views::Widget::SchedulePaintInRect(rect);

  // Paint the frame caption area and window controls during immersive reveal.
  if (browser_view_ &&
      browser_view_->immersive_mode_controller()->IsRevealed()) {
    // This function should not be reentrant because the TopContainerView
    // paints to a layer for the duration of the immersive reveal.
    views::View* top_container = browser_view_->top_container();
    CHECK(top_container->layer());
    top_container->SchedulePaintInRect(rect);
  }
}

void BrowserFrame::OnNativeWidgetActivationChanged(bool active) {
  if (active) {
    // When running under remote desktop, if the remote desktop client is not
    // active on the users desktop, then none of the windows contained in the
    // remote desktop will be activated.  However, NativeWidget::Activate() will
    // still bring this browser window to the foreground.  We explicitly set
    // ourselves as the last active browser window to ensure that we get treated
    // as such by the rest of Chrome.
    BrowserList::SetLastActive(browser_view_->browser());
  }
  Widget::OnNativeWidgetActivationChanged(active);
}

void BrowserFrame::ShowContextMenuForView(views::View* source,
                                          const gfx::Point& p,
                                          ui::MenuSourceType source_type) {
  if (chrome::IsRunningInForcedAppMode())
    return;

  // Only show context menu if point is in unobscured parts of browser, i.e.
  // if NonClientHitTest returns :
  // - HTCAPTION: in title bar or unobscured part of tabstrip
  // - HTNOWHERE: as the name implies.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(non_client_view(), &point_in_view_coords);
  int hit_test = non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION || hit_test == HTNOWHERE) {
    menu_runner_.reset(new views::MenuRunner(
        GetSystemMenuModel(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
    if (menu_runner_->RunMenuAt(source->GetWidget(),
                                NULL,
                                gfx::Rect(p, gfx::Size(0, 0)),
                                views::MENU_ANCHOR_TOPLEFT,
                                source_type) ==
        views::MenuRunner::MENU_DELETED) {
      return;
    }
  }
}

ui::MenuModel* BrowserFrame::GetSystemMenuModel() {
#if defined(OS_CHROMEOS)
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  if (delegate && delegate->NumberOfLoggedInUsers() > 1) {
    // In Multi user mode, the number of users as well as the order of users
    // can change. Coming here we have more then one user and since the menu
    // model contains the user information, it must get updated to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
  }
#endif
  if (!menu_model_builder_.get()) {
    menu_model_builder_.reset(
        new SystemMenuModelBuilder(browser_view_, browser_view_->browser()));
    menu_model_builder_->Init();
  }
  return menu_model_builder_->menu_model();
}

AvatarMenuButton* BrowserFrame::GetAvatarMenuButton() {
  return browser_frame_view_->avatar_button();
}

NewAvatarButton* BrowserFrame::GetNewAvatarMenuButton() {
  return browser_frame_view_->new_avatar_button();
}

bool BrowserFrame::ShouldLeaveOffsetNearTopBorder() {
  return !IsMaximized();
}

void BrowserFrame::OnUseCustomChromeFrameChanged() {
  // Tell the window manager to add or remove system borders.
  set_frame_type(UseCustomFrame() ? Widget::FRAME_TYPE_FORCE_CUSTOM
                                  : Widget::FRAME_TYPE_FORCE_NATIVE);
  FrameTypeChanged();
}
