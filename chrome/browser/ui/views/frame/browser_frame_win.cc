// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_win.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <set>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/win/metro.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/layout.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/menu/native_menu_win.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_win.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "webkit/glue/window_open_disposition.h"
#include "win8/util/win8_util.h"

#pragma comment(lib, "dwmapi.lib")

// static
static const int kClientEdgeThickness = 3;
static const int kTabDragWindowAlpha = 200;
// We need to offset the DWMFrame into the toolbar so that the blackness
// doesn't show up on our rounded corners.
static const int kDWMFrameTopOffset = 3;
// If not -1, windows are shown with this state.
static int explicit_show_state = -1;

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

#if !defined(USE_AURA)
extern "C" {
// Windows metro exported functions from metro_driver.
typedef void (*SetFrameWindow)(HWND window);
typedef void (*CloseFrameWindow)(HWND window);
typedef void (*FlipFrameWindows)();
typedef void (*MetroSetFullscreen)(bool fullscreen);
}
#endif  // USE_AURA

views::Button* MakeWindowSwitcherButton(views::ButtonListener* listener,
                                        bool is_off_the_record) {
  views::ImageButton* switcher_button = new views::ImageButton(listener);
  // The button in the incognito window has the hot-cold images inverted
  // with respect to the regular browser window.
  switcher_button->SetImage(
      views::ImageButton::STATE_NORMAL,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          is_off_the_record ? IDR_INCOGNITO_SWITCH_ON :
                              IDR_INCOGNITO_SWITCH_OFF));
  switcher_button->SetImage(
      views::ImageButton::STATE_HOVERED,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          is_off_the_record ? IDR_INCOGNITO_SWITCH_OFF :
                              IDR_INCOGNITO_SWITCH_ON));
  switcher_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                     views::ImageButton::ALIGN_MIDDLE);
  return switcher_button;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, public:

BrowserFrameWin::BrowserFrameWin(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetWin(browser_frame),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
  if (win8::IsSingleWindowMetroMode()) {
    browser_view->SetWindowSwitcherButton(
        MakeWindowSwitcherButton(this, browser_view->IsOffTheRecord()));
  }
}

BrowserFrameWin::~BrowserFrameWin() {
}

// static
void BrowserFrameWin::SetShowState(int state) {
  explicit_show_state = state;
}

void BrowserFrameWin::AdjustFrameForImmersiveMode() {
#if defined(USE_AURA)
  return;
#endif  // USE_AURA
  HMODULE metro = base::win::GetMetroModule();
  if (!metro)
    return;
  // We are in metro mode.
  browser_frame_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  SetFrameWindow set_frame_window = reinterpret_cast<SetFrameWindow>(
      ::GetProcAddress(metro, "SetFrameWindow"));
  set_frame_window(browser_frame_->GetNativeWindow());
}

void BrowserFrameWin::CloseImmersiveFrame() {
#if defined(USE_AURA)
  return;
#endif  // USE_AURA
  HMODULE metro = base::win::GetMetroModule();
  if (!metro)
    return;
  CloseFrameWindow close_frame_window = reinterpret_cast<CloseFrameWindow>(
      ::GetProcAddress(metro, "CloseFrameWindow"));
  close_frame_window(browser_frame_->GetNativeWindow());
}


views::NativeMenuWin* BrowserFrameWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_.reset(
        new views::NativeMenuWin(browser_frame_->GetSystemMenuModel(),
                                 GetNativeView()));
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, views::NativeWidgetWin overrides:

int BrowserFrameWin::GetInitialShowState() const {
  if (explicit_show_state != -1)
    return explicit_show_state;

  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

bool BrowserFrameWin::GetClientAreaInsets(gfx::Insets* insets) const {
  // Use the default client insets for an opaque frame or a glass popup/app
  // frame.
  if (!GetWidget()->ShouldUseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal()) {
    return false;
  }

  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  // In fullscreen mode, we have no frame. In restored mode, we draw our own
  // client edge over part of the default frame.
  if (IsFullscreen())
    border_thickness = 0;
  else if (!IsMaximized())
    border_thickness -= kClientEdgeThickness;
  insets->Set(0, border_thickness, border_thickness, border_thickness);
  return true;
}

void BrowserFrameWin::HandleFrameChanged() {
  // Handle window frame layout changes, then set the updated glass region.
  NativeWidgetWin::HandleFrameChanged();
  UpdateDWMFrame();
}

bool BrowserFrameWin::PreHandleMSG(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   LRESULT* result) {
  static const UINT metro_navigation_search_message =
      RegisterWindowMessage(chrome::kMetroNavigationAndSearchMessage);

  static const UINT metro_get_current_tab_info_message =
      RegisterWindowMessage(chrome::kMetroGetCurrentTabInfoMessage);

  if (message == metro_navigation_search_message) {
    HandleMetroNavSearchRequest(w_param, l_param);
    return false;
  } else if (message == metro_get_current_tab_info_message) {
    GetMetroCurrentTabInfo(w_param);
    return false;
  }

  switch (message) {
  case WM_ACTIVATE:
    if (LOWORD(w_param) != WA_INACTIVE)
      minimize_button_metrics_.OnHWNDActivated();
    return false;
  case WM_PRINT:
    if (win8::IsSingleWindowMetroMode()) {
      // This message is sent by the AnimateWindow API which is used in metro
      // mode to flip between active chrome windows.
      RECT client_rect = {0};
      ::GetClientRect(GetNativeView(), &client_rect);
      HDC dest_dc = reinterpret_cast<HDC>(w_param);
      DCHECK(dest_dc);
      HDC src_dc = ::GetDC(GetNativeView());
      ::BitBlt(dest_dc, 0, 0, client_rect.right - client_rect.left,
               client_rect.bottom - client_rect.top, src_dc, 0, 0,
               SRCCOPY);
      ::ReleaseDC(GetNativeView(), src_dc);
      *result = 0;
      return true;
    }
    return false;
  case WM_ENDSESSION:
    browser::SessionEnding();
    return true;
  case WM_INITMENUPOPUP:
    GetSystemMenu()->UpdateStates();
    return true;
  }
  return false;
}

void BrowserFrameWin::PostHandleMSG(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {
  switch (message) {
  case WM_CREATE:
    minimize_button_metrics_.Init(GetNativeView());
    break;
  case WM_WINDOWPOSCHANGED:
    UpdateDWMFrame();

    // Windows lies to us about the position of the minimize button before a
    // window is visible.  We use this position to place the OTR avatar in RTL
    // mode, so when the window is shown, we need to re-layout and schedule a
    // paint for the non-client frame view so that the icon top has the correct
    // position when the window becomes visible.  This fixes bugs where the icon
    // appears to overlay the minimize button.
    // Note that we will call Layout every time SetWindowPos is called with
    // SWP_SHOWWINDOW, however callers typically are careful about not
    // specifying this flag unless necessary to avoid flicker.
    // This may be invoked during creation on XP and before the non_client_view
    // has been created.
    WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(l_param);
    if (window_pos->flags & SWP_SHOWWINDOW && GetWidget()->non_client_view()) {
      GetWidget()->non_client_view()->Layout();
      GetWidget()->non_client_view()->SchedulePaint();
    }
    break;
  }
}

void BrowserFrameWin::OnScreenReaderDetected() {
  content::BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
  NativeWidgetWin::OnScreenReaderDetected();
}

bool BrowserFrameWin::ShouldUseNativeFrame() const {
  // App panel windows draw their own frame.
  if (browser_view_->IsPanel())
    return false;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal() &&
      NativeWidgetWin::ShouldUseNativeFrame()) {
    return true;
  }

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetWidget()->GetThemeProvider()->ShouldUseNativeFrame();
}

void BrowserFrameWin::Show() {
  AdjustFrameForImmersiveMode();
  views::NativeWidgetWin::Show();
}

void BrowserFrameWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  AdjustFrameForImmersiveMode();
  views::NativeWidgetWin::ShowMaximizedWithBounds(restored_bounds);
}

void BrowserFrameWin::ShowWithWindowState(ui::WindowShowState show_state) {
  AdjustFrameForImmersiveMode();
  views::NativeWidgetWin::ShowWithWindowState(show_state);
}

void BrowserFrameWin::Close() {
  CloseImmersiveFrame();
  views::NativeWidgetWin::Close();
}

void BrowserFrameWin::FrameTypeChanged() {
  // In Windows 8 metro mode the frame type is set to FRAME_TYPE_FORCE_CUSTOM
  // by default. We reset it back to FRAME_TYPE_DEFAULT to ensure that we
  // don't end up defaulting to BrowserNonClientFrameView in all cases.
  if (win8::IsSingleWindowMetroMode())
    browser_frame_->set_frame_type(views::Widget::FRAME_TYPE_DEFAULT);

  views::NativeWidgetWin::FrameTypeChanged();

  // In Windows 8 metro mode we call Show on the BrowserFrame instance to
  // ensure that the window can be styled appropriately, i.e. no sysmenu,
  // etc.
  if (win8::IsSingleWindowMetroMode())
    Show();
}

void BrowserFrameWin::SetFullscreen(bool fullscreen) {
  if (win8::IsSingleWindowMetroMode()) {
    HMODULE metro = base::win::GetMetroModule();
    if (metro) {
      MetroSetFullscreen set_full_screen = reinterpret_cast<MetroSetFullscreen>(
        ::GetProcAddress(metro, "SetFullscreen"));
      DCHECK(set_full_screen);
      if (set_full_screen)
        set_full_screen(fullscreen);
    } else {
      NOTREACHED() << "Failed to get metro driver module";
    }
  }
  views::NativeWidgetWin::SetFullscreen(fullscreen);
}

void BrowserFrameWin::Activate() {
  // In Windows 8 metro mode we have only one window visible at any given time.
  // The Activate code path is typically called when a new browser window is
  // being activated. In metro we need to ensure that the window currently
  // being displayed is hidden and the new window being activated becomes
  // visible. This is achieved by calling AdjustFrameForImmersiveMode()
  // followed by ShowWindow().
  if (win8::IsSingleWindowMetroMode()) {
    AdjustFrameForImmersiveMode();
    ::ShowWindow(browser_frame_->GetNativeWindow(), SW_SHOWNORMAL);
  } else {
    views::NativeWidgetWin::Activate();
  }
}


////////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameWin::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameWin::AsNativeWidget() const {
  return this;
}

bool BrowserFrameWin::UsesNativeSystemMenu() const {
  return true;
}

int BrowserFrameWin::GetMinimizeButtonOffset() const {
  return minimize_button_metrics_.GetMinimizeButtonOffsetX();
}

void BrowserFrameWin::TabStripDisplayModeChanged() {
  UpdateDWMFrame();
}

void BrowserFrameWin::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  HMODULE metro = base::win::GetMetroModule();
  if (!metro)
    return;

  // Toggle the profile and switch to the corresponding browser window in the
  // profile. The GetOffTheRecordProfile function is documented to create an
  // incognito profile if one does not exist. That is not a concern as the
  // windows 8 window switcher button shows up on the caption only when a
  // normal window and an incognito window are open simultaneously.
  Profile* profile_to_switch_to = NULL;
  Profile* current_profile = browser_view()->browser()->profile();
  if (current_profile->IsOffTheRecord())
    profile_to_switch_to = current_profile->GetOriginalProfile();
  else
    profile_to_switch_to = current_profile->GetOffTheRecordProfile();

  DCHECK(profile_to_switch_to);

  Browser* browser_to_switch_to = chrome::FindTabbedBrowser(
      profile_to_switch_to, false, chrome::HOST_DESKTOP_TYPE_NATIVE);

  DCHECK(browser_to_switch_to);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
      browser_to_switch_to);

  // Tell the metro_driver to switch to the Browser we found above. This
  // causes the current browser window to be hidden.
  SetFrameWindow set_frame_window = reinterpret_cast<SetFrameWindow>(
      ::GetProcAddress(metro, "SetFrameWindow"));
  set_frame_window(browser_view->frame()->GetNativeWindow());
  ::ShowWindow(browser_view->frame()->GetNativeWindow(), SW_SHOWNORMAL);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, private:

void BrowserFrameWin::UpdateDWMFrame() {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking browser_frame_->ShouldUseNativeFrame() here.  Using that here
  // would mean we wouldn't reset the glass area to zero when moving from the
  // native frame to an opaque frame, leading to graphical glitches behind the
  // opaque frame.  Instead, we use that function below to tell us whether the
  // frame is currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->IsBrowserTypeNormal() ||
      !NativeWidgetWin::ShouldUseNativeFrame())
    return;

  MARGINS margins = { 0 };

  // If the opaque frame is visible, we use the default (zero) margins.
  // Otherwise, we need to figure out how to extend the glass in.
  if (browser_frame_->ShouldUseNativeFrame()) {
    // In fullscreen mode, we don't extend glass into the client area at all,
    // because the GDI-drawn text in the web content composited over it will
    // become semi-transparent over any glass area.
    if (!IsMaximized() && !IsFullscreen()) {
      margins.cxLeftWidth = kClientEdgeThickness + 1;
      margins.cxRightWidth = kClientEdgeThickness + 1;
      margins.cyBottomHeight = kClientEdgeThickness + 1;
      margins.cyTopHeight = kClientEdgeThickness + 1;
    }
    // In maximized mode, we only have a titlebar strip of glass, no side/bottom
    // borders.
    if (!IsFullscreen()) {
      gfx::Rect tabstrip_bounds(
          browser_frame_->GetBoundsForTabStrip(browser_view_->tabstrip()));
      margins.cyTopHeight = tabstrip_bounds.bottom() + kDWMFrameTopOffset;
    }
  }

  DwmExtendFrameIntoClientArea(GetNativeView(), &margins);
}

void BrowserFrameWin::HandleMetroNavSearchRequest(WPARAM w_param,
                                                  LPARAM l_param) {
  if (!base::win::IsMetroProcess()) {
    NOTREACHED() << "Received unexpected metro navigation request";
    return;
  }

  if (!w_param && !l_param) {
    NOTREACHED() << "Invalid metro request parameters";
    return;
  }

  Browser* browser = browser_view()->browser();
  DCHECK(browser);

  GURL request_url;

  if (w_param) {
    const wchar_t* url = reinterpret_cast<const wchar_t*>(w_param);
    request_url = GURL(url);
  } else if (l_param) {
    const wchar_t* search_string =
        reinterpret_cast<const wchar_t*>(l_param);
    const TemplateURL* default_provider =
        TemplateURLServiceFactory::GetForProfile(browser->profile())->
        GetDefaultSearchProvider();
    if (default_provider) {
      const TemplateURLRef& search_url = default_provider->url_ref();
      DCHECK(search_url.SupportsReplacement());
      request_url = GURL(search_url.ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(search_string)));
    }
  }
  if (request_url.is_valid()) {
    browser->OpenURL(OpenURLParams(request_url, Referrer(), NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_TYPED, false));
  }
}

void BrowserFrameWin::GetMetroCurrentTabInfo(WPARAM w_param) {
  if (!base::win::IsMetroProcess()) {
    NOTREACHED() << "Received unexpected metro request";
    return;
  }

  if (!w_param) {
    NOTREACHED() << "Invalid metro request parameter";
    return;
  }

  base::win::CurrentTabInfo* current_tab_info =
      reinterpret_cast<base::win::CurrentTabInfo*>(w_param);

  Browser* browser = browser_view()->browser();
  DCHECK(browser);

  // We allocate memory for the title and url via LocalAlloc. The caller has to
  // free the memory via LocalFree.
  current_tab_info->title = base::win::LocalAllocAndCopyString(
      browser->GetWindowTitleForCurrentTab());

  WebContents* current_tab = chrome::GetActiveWebContents(browser);
  DCHECK(current_tab);

  current_tab_info->url = base::win::LocalAllocAndCopyString(
      UTF8ToWide(current_tab->GetURL().spec()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font* title_font =
      new gfx::Font(views::NativeWidgetWin::GetWindowTitleFont());
  return *title_font;
}

bool BrowserFrame::ShouldLeaveOffsetNearTopBorder() {
  if (win8::IsSingleWindowMetroMode()) {
    if (ui::GetDisplayLayout() == ui::LAYOUT_DESKTOP)
      return false;
  }
  return !IsMaximized();
}

////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameWin(browser_frame, browser_view);
}
