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
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/system_menu_model.h"
#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"
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
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/menu/native_menu_win.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_win.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "webkit/glue/window_open_disposition.h"

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
typedef void (*SetFrameWindow)(HWND window);
typedef void (*CloseFrameWindow)(HWND window);
}
#endif  // USE_AURA

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, public:

BrowserFrameWin::BrowserFrameWin(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetWin(browser_frame),
      browser_view_(browser_view),
      browser_frame_(browser_frame),
      system_menu_delegate_(new SystemMenuModelDelegate(browser_view,
          browser_view->browser())) {
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


///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, views::NativeWidgetWin overrides:

int BrowserFrameWin::GetShowState() const {
  if (explicit_show_state != -1)
    return explicit_show_state;

  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

gfx::Insets BrowserFrameWin::GetClientAreaInsets() const {
  // Use the default client insets for an opaque frame or a glass popup/app
  // frame.
  if (!GetWidget()->ShouldUseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal()) {
    return NativeWidgetWin::GetClientAreaInsets();
  }

  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  // In fullscreen mode, we have no frame. In restored mode, we draw our own
  // client edge over part of the default frame.
  if (IsFullscreen())
    border_thickness = 0;
  else if (!IsMaximized())
    border_thickness -= kClientEdgeThickness;
  return gfx::Insets(0, border_thickness, border_thickness, border_thickness);
}

void BrowserFrameWin::UpdateFrameAfterFrameChange() {
  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  UpdateDWMFrame();
  NativeWidgetWin::UpdateFrameAfterFrameChange();
}

void BrowserFrameWin::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::SessionEnding();
}

void BrowserFrameWin::OnInitMenuPopup(HMENU menu, UINT position,
                                      BOOL is_system_menu) {
  system_menu_->UpdateStates();
}

void BrowserFrameWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  NativeWidgetWin::OnWindowPosChanged(window_pos);
  UpdateDWMFrame();

  // Windows lies to us about the position of the minimize button before a
  // window is visible.  We use this position to place the OTR avatar in RTL
  // mode, so when the window is shown, we need to re-layout and schedule a
  // paint for the non-client frame view so that the icon top has the correct
  // position when the window becomes visible.  This fixes bugs where the icon
  // appears to overlay the minimize button.
  // Note that we will call Layout every time SetWindowPos is called with
  // SWP_SHOWWINDOW, however callers typically are careful about not specifying
  // this flag unless necessary to avoid flicker.
  // This may be invoked during creation on XP and before the non_client_view
  // has been created.
  if (window_pos->flags & SWP_SHOWWINDOW && GetWidget()->non_client_view()) {
    GetWidget()->non_client_view()->Layout();
    GetWidget()->non_client_view()->SchedulePaint();
  }
}

void BrowserFrameWin::OnScreenReaderDetected() {
  BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
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

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameWin::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameWin::AsNativeWidget() const {
  return this;
}

void BrowserFrameWin::InitSystemContextMenu() {
  system_menu_contents_.reset(new SystemMenuModel(system_menu_delegate_.get()));
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (browser_view_->IsBrowserTypeNormal())
    BuildSystemMenuForBrowserWindow();
  else
    BuildSystemMenuForAppOrPopupWindow();
  system_menu_.reset(
      new views::NativeMenuWin(system_menu_contents_.get(), GetNativeWindow()));
  system_menu_->Rebuild();
}

int BrowserFrameWin::GetMinimizeButtonOffset() const {
  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(GetNativeView(), WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  CPoint minimize_button_corner(titlebar_info.rgrect[2].left,
                                titlebar_info.rgrect[2].top);
  MapWindowPoints(HWND_DESKTOP, GetNativeView(), &minimize_button_corner, 1);

  return minimize_button_corner.x;
}

void BrowserFrameWin::TabStripDisplayModeChanged() {
  UpdateDWMFrame();
}

LRESULT BrowserFrameWin::OnWndProc(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param) {
  static const UINT metro_navigation_search_message =
      RegisterWindowMessage(chrome::kMetroNavigationAndSearchMessage);

  static const UINT metro_get_current_tab_info_message =
      RegisterWindowMessage(chrome::kMetroGetCurrentTabInfoMessage);

  if (message == metro_navigation_search_message) {
    HandleMetroNavSearchRequest(w_param, l_param);
  } else if (message == metro_get_current_tab_info_message) {
    GetMetroCurrentTabInfo(w_param);
  }
  return views::NativeWidgetWin::OnWndProc(message, w_param, l_param);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, private:

void BrowserFrameWin::UpdateDWMFrame() {
  // Nothing to do yet, or we're not showing a DWM frame.
  if (!GetWidget()->client_view() || !browser_frame_->ShouldUseNativeFrame())
    return;

  MARGINS margins = { 0 };
  if (browser_view_->IsBrowserTypeNormal()) {
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
    if (!browser_view_->IsFullscreen()) {
      gfx::Rect tabstrip_bounds(
          browser_frame_->GetBoundsForTabStrip(browser_view_->tabstrip()));
      margins.cyTopHeight = tabstrip_bounds.bottom() + kDWMFrameTopOffset;
    }
  } else {
    // For popup and app windows we want to use the default margins.
  }
  DwmExtendFrameIntoClientArea(GetNativeView(), &margins);
}

void BrowserFrameWin::BuildSystemMenuForBrowserWindow() {
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                             IDS_TASK_MANAGER);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddFrameToggleItems();
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void BrowserFrameWin::BuildSystemMenuForAppOrPopupWindow() {
  Browser* browser = browser_view()->browser();
  if (browser->is_app()) {
    system_menu_contents_->AddSeparator();
    system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                               IDS_TASK_MANAGER);
  }
  system_menu_contents_->AddSeparator();
  encoding_menu_contents_.reset(new EncodingMenuModel(browser));
  system_menu_contents_->AddSubMenuWithStringId(IDC_ENCODING_MENU,
                                                IDS_ENCODING_MENU,
                                                encoding_menu_contents_.get());
  zoom_menu_contents_.reset(new ZoomMenuModel(system_menu_delegate_.get()));
  system_menu_contents_->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                                zoom_menu_contents_.get());
  system_menu_contents_->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  system_menu_contents_->AddItemWithStringId(IDC_FIND, IDS_FIND);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  system_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
  system_menu_contents_->AddItemWithStringId(IDC_CUT, IDS_CUT);
  system_menu_contents_->AddSeparator();
  if (browser->is_app()) {
    system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB,
                                               IDS_APP_MENU_NEW_WEB_PAGE);
  } else {
    system_menu_contents_->AddItemWithStringId(IDC_SHOW_AS_TAB,
                                               IDS_SHOW_AS_TAB);
  }
  system_menu_contents_->AddItemWithStringId(IDC_COPY_URL,
                                             IDS_APP_MENU_COPY_URL);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  system_menu_contents_->AddItemWithStringId(IDC_FORWARD,
                                             IDS_CONTENT_CONTEXT_FORWARD);
  system_menu_contents_->AddItemWithStringId(IDC_BACK,
                                             IDS_CONTENT_CONTEXT_BACK);
  AddFrameToggleItems();
}

void BrowserFrameWin::AddFrameToggleItems() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugEnableFrameToggle)) {
    system_menu_contents_->AddSeparator();
    system_menu_contents_->AddItem(IDC_DEBUG_FRAME_TOGGLE,
                                   L"Toggle Frame Type");
  }
}

void BrowserFrameWin::HandleMetroNavSearchRequest(WPARAM w_param,
                                                  LPARAM l_param) {
  if (!base::win::GetMetroModule()) {
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
      request_url = GURL(search_url.ReplaceSearchTerms(search_string,
                            TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
                            string16()));
    }
  }
  if (request_url.is_valid()) {
    browser->OpenURL(OpenURLParams(request_url, Referrer(), NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_TYPED, false));
  }
}

void BrowserFrameWin::GetMetroCurrentTabInfo(WPARAM w_param) {
  if (!base::win::GetMetroModule()) {
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

  WebContents* current_tab = browser->GetSelectedWebContents();
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

////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameWin(browser_frame, browser_view);
}
