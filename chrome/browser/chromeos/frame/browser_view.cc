// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/frame/panel_browser_view.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/views/frame/browser_frame_gtk.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/theme_background.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/window/hit_test.h"
#include "views/window/window.h"

namespace {

// Amount to offset the toolbar by when vertical tabs are enabled.
const int kVerticalTabStripToolbarOffset = 2;
// Amount to tweak the position of the status area to get it to look right.
const int kStatusAreaVerticalAdjustment = -1;

// If a popup window is larger than this fraction of the screen, create a tab.
const float kPopupMaxWidthFactor = 0.5;
const float kPopupMaxHeightFactor = 0.6;

// This MenuItemView delegate class forwards to the
// SimpleMenuModel::Delegate() implementation.
class SimpleMenuModelDelegateAdapter : public views::MenuDelegate {
 public:
  explicit SimpleMenuModelDelegateAdapter(
      ui::SimpleMenuModel::Delegate* simple_menu_model_delegate);

  // views::MenuDelegate implementation.
  virtual bool GetAccelerator(int id,
                              views::Accelerator* accelerator) OVERRIDE;
  virtual std::wstring GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

 private:
  ui::SimpleMenuModel::Delegate* simple_menu_model_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMenuModelDelegateAdapter);
};

// SimpleMenuModelDelegateAdapter:
SimpleMenuModelDelegateAdapter::SimpleMenuModelDelegateAdapter(
    ui::SimpleMenuModel::Delegate* simple_menu_model_delegate)
    : simple_menu_model_delegate_(simple_menu_model_delegate) {
}

// SimpleMenuModelDelegateAdapter, views::MenuDelegate implementation.

bool SimpleMenuModelDelegateAdapter::GetAccelerator(
    int id,
    views::Accelerator* accelerator) {
  return simple_menu_model_delegate_->GetAcceleratorForCommandId(
      id, accelerator);
}

std::wstring SimpleMenuModelDelegateAdapter::GetLabel(int id) const {
  return UTF16ToWide(simple_menu_model_delegate_->GetLabelForCommandId(id));
}

bool SimpleMenuModelDelegateAdapter::IsCommandEnabled(int id) const {
  return simple_menu_model_delegate_->IsCommandIdEnabled(id);
}

bool SimpleMenuModelDelegateAdapter::IsItemChecked(int id) const {
  return simple_menu_model_delegate_->IsCommandIdChecked(id);
}

void SimpleMenuModelDelegateAdapter::ExecuteCommand(int id) {
  simple_menu_model_delegate_->ExecuteCommand(id);
}

}  // namespace

namespace chromeos {

// LayoutManager for BrowserView, which layouts extra components such as
// the status views as follows:
//       ____  __ __
//      /    \   \  \     [StatusArea]
//
class BrowserViewLayout : public ::BrowserViewLayout {
 public:
  BrowserViewLayout() : ::BrowserViewLayout() {}
  virtual ~BrowserViewLayout() {}

  //////////////////////////////////////////////////////////////////////////////
  // BrowserViewLayout overrides:

  void Installed(views::View* host) {
    status_area_ = NULL;
    ::BrowserViewLayout::Installed(host);
  }

  void ViewAdded(views::View* host,
                 views::View* view) {
    ::BrowserViewLayout::ViewAdded(host, view);
    switch (view->GetID()) {
      case VIEW_ID_STATUS_AREA:
        status_area_ = static_cast<chromeos::StatusAreaView*>(view);
        break;
    }
  }

  // In the normal and the compact navigation bar mode, ChromeOS
  // layouts compact navigation buttons and status views in the title
  // area. See Layout
  virtual int LayoutTabStripRegion() OVERRIDE {
    if (browser_view_->IsFullscreen() || !browser_view_->IsTabStripVisible()) {
      status_area_->SetVisible(false);
      tabstrip_->SetVisible(false);
      tabstrip_->SetBounds(0, 0, 0, 0);
      return 0;
    }

    gfx::Rect tabstrip_bounds(
        browser_view_->frame()->GetBoundsForTabStrip(tabstrip_));
    gfx::Point tabstrip_origin = tabstrip_bounds.origin();
    views::View::ConvertPointToView(browser_view_->parent(), browser_view_,
                                    &tabstrip_origin);
    tabstrip_bounds.set_origin(tabstrip_origin);
    return browser_view_->UseVerticalTabs() ?
        LayoutTitlebarComponentsWithVerticalTabs(tabstrip_bounds) :
        LayoutTitlebarComponents(tabstrip_bounds);
  }

  virtual int LayoutToolbar(int top) OVERRIDE {
    if (!browser_view_->IsFullscreen() && browser_view_->IsTabStripVisible() &&
        browser_view_->UseVerticalTabs()) {
      // For vertical tabs the toolbar is positioned in
      // LayoutTitlebarComponentsWithVerticalTabs.
      return top;
    }
    return ::BrowserViewLayout::LayoutToolbar(top);
  }

  virtual bool IsPositionInWindowCaption(const gfx::Point& point) OVERRIDE {
    return ::BrowserViewLayout::IsPositionInWindowCaption(point)
        && !IsPointInViewsInTitleArea(point);
  }

  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE {
    gfx::Point point_in_browser_view_coords(point);
    views::View::ConvertPointToView(
        browser_view_->parent(), browser_view_,
        &point_in_browser_view_coords);
    return IsPointInViewsInTitleArea(point_in_browser_view_coords) ?
        HTCLIENT : ::BrowserViewLayout::NonClientHitTest(point);
  }

 private:
  chromeos::BrowserView* chromeos_browser_view() {
    return static_cast<chromeos::BrowserView*>(browser_view_);
  }

  // Tests if the point is on one of views that are within the
  // considered title bar area of client view.
  bool IsPointInViewsInTitleArea(const gfx::Point& point)
      const {
    gfx::Point point_in_status_area_coords(point);
    views::View::ConvertPointToView(browser_view_, status_area_,
                                    &point_in_status_area_coords);
    if (status_area_->HitTest(point_in_status_area_coords))
      return true;

    return false;
  }

  // Positions the titlebar, toolbar and tabstrip. This is
  // used when side tabs are enabled.
  int LayoutTitlebarComponentsWithVerticalTabs(const gfx::Rect& bounds) {
    if (bounds.IsEmpty())
      return 0;

    tabstrip_->SetVisible(true);
    status_area_->SetVisible(true);

    gfx::Size status_size = status_area_->GetPreferredSize();
    int status_height = status_size.height();

    int status_x = bounds.x();
    // Layout the status area.
    status_area_->SetBounds(status_x, bounds.bottom() - status_height,
                            status_size.width(), status_height);

    // The tabstrip's width is the bigger of it's preferred width and the width
    // the status area.
    int tabstrip_w = std::max(status_x + status_size.width(),
                              tabstrip_->GetPreferredSize().width());
    tabstrip_->SetBounds(bounds.x(), bounds.y(), tabstrip_w,
                         bounds.height() - status_height);

    // The toolbar is promoted to the title for vertical tabs.
    bool toolbar_visible = browser_view_->IsToolbarVisible();
    int toolbar_height = 0;
    if (toolbar_) {
      toolbar_->SetVisible(toolbar_visible);
      if (toolbar_visible)
        toolbar_height = toolbar_->GetPreferredSize().height();
      int tabstrip_max_x = tabstrip_->bounds().right();
      toolbar_->SetBounds(tabstrip_max_x,
                          bounds.y() - kVerticalTabStripToolbarOffset,
                          browser_view_->width() - tabstrip_max_x,
                          toolbar_height);
    }
    // Adjust the available bounds for other components.
    gfx::Rect available_bounds = vertical_layout_rect();
    available_bounds.Inset(tabstrip_w, 0, 0, 0);
    set_vertical_layout_rect(available_bounds);
    return bounds.y() + toolbar_height;
  }

  // Lays out tabstrip and status area in the title bar area (given by
  // |bounds|).
  int LayoutTitlebarComponents(const gfx::Rect& bounds) {
    if (bounds.IsEmpty())
      return 0;

    tabstrip_->SetVisible(true);
    status_area_->SetVisible(true);

    // Layout status area after tab strip.
    gfx::Size status_size = status_area_->GetPreferredSize();
    status_area_->SetBounds(
        bounds.right() - status_size.width(),
        bounds.y() + kStatusAreaVerticalAdjustment,
        status_size.width(),
        status_size.height());
    tabstrip_->SetBounds(bounds.x(), bounds.y(),
        std::max(0, status_area_->bounds().x() - bounds.x()),
        bounds.height());
    return bounds.bottom();
  }

  chromeos::StatusAreaView* status_area_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

// BrowserView

BrowserView::BrowserView(Browser* browser)
    : ::BrowserView(browser),
      status_area_(NULL),
      saved_focused_widget_(NULL) {
  system_menu_delegate_.reset(new SimpleMenuModelDelegateAdapter(this));
}

BrowserView::~BrowserView() {
  if (toolbar())
    toolbar()->RemoveMenuListener(this);
}

// BrowserView, ::BrowserView overrides:

void BrowserView::Init() {
  ::BrowserView::Init();
  status_area_ = new StatusAreaView(this);
  status_area_->SetID(VIEW_ID_STATUS_AREA);
  AddChildView(status_area_);
  status_area_->Init();

  frame()->non_client_view()->SetContextMenuController(this);

  // Listen to wrench menu opens.
  if (toolbar())
    toolbar()->AddMenuListener(this);

  // Make sure the window is set to the right type.
  std::vector<int> params;
  params.push_back(browser()->tab_count());
  params.push_back(browser()->active_index());
  params.push_back(gtk_get_current_event_time());
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(frame()->GetNativeWindow()),
      WM_IPC_WINDOW_CHROME_TOPLEVEL,
      &params);
}

void BrowserView::Show() {
  ShowInternal(true);
}

void BrowserView::ShowInactive() {
  ShowInternal(false);
}

void BrowserView::ShowInternal(bool is_active) {
  bool was_visible = frame()->IsVisible();
  if (is_active)
    ::BrowserView::Show();
  else
    ::BrowserView::ShowInactive();
  if (!was_visible) {
    // Have to update the tab count and selected index to reflect reality.
    std::vector<int> params;
    params.push_back(browser()->tab_count());
    params.push_back(browser()->active_index());
    WmIpc::instance()->SetWindowType(
        GTK_WIDGET(frame()->GetNativeWindow()),
        WM_IPC_WINDOW_CHROME_TOPLEVEL,
        &params);
  }
}

void BrowserView::FocusChromeOSStatus() {
  SaveFocusedView();
  status_area_->SetPaneFocus(last_focused_view_storage_id(), NULL);
}

views::LayoutManager* BrowserView::CreateLayoutManager() const {
  return new BrowserViewLayout();
}

void BrowserView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

bool BrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeosFrame)) {
    // Typically we don't request a full screen size. This means we'll request a
    // non-full screen size, layout/paint at that size, then the window manager
    // will snap us to full screen size. This results in an ugly
    // resize/paint. To avoid this we always request a full screen size.
    *bounds = views::Screen::GetMonitorWorkAreaNearestWindow(
        GTK_WIDGET(GetWindow()->GetNativeWindow()));
    return true;
  }
  return ::BrowserView::GetSavedWindowBounds(bounds);
}

void BrowserView::Cut() {
  gtk_util::DoCut(this);
}

void BrowserView::Copy() {
  gtk_util::DoCopy(this);
}

void BrowserView::Paste() {
  gtk_util::DoPaste(this);
}

WindowOpenDisposition BrowserView::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  // If a popup is larger than a given fraction of the screen, turn it into
  // a foreground tab. Also check for width or height == 0, which would
  // indicate a tab sized popup window.
  GdkScreen* screen = gdk_screen_get_default();
  int max_width = gdk_screen_get_width(screen) * kPopupMaxWidthFactor;
  int max_height = gdk_screen_get_height(screen) * kPopupMaxHeightFactor;
  if (bounds.width() > max_width ||
      bounds.width() == 0 ||
      bounds.height() > max_height ||
      bounds.height() == 0) {
    return NEW_FOREGROUND_TAB;
  }
  return NEW_POPUP;
}

// views::ContextMenuController implementation.
void BrowserView::ShowContextMenuForView(views::View* source,
                                         const gfx::Point& p,
                                         bool is_mouse_gesture) {
  // Only show context menu if point is in unobscured parts of browser, i.e.
  // if NonClientHitTest returns :
  // - HTCAPTION: in title bar or unobscured part of tabstrip
  // - HTNOWHERE: as the name implies.
  gfx::Point point_in_parent_coords(p);
  views::View::ConvertPointToView(NULL, parent(), &point_in_parent_coords);
  int hit_test = NonClientHitTest(point_in_parent_coords);
  if (hit_test == HTCAPTION || hit_test == HTNOWHERE) {
    // rebuild menu so it reflects current application state
    InitSystemMenu();
    system_menu_->RunMenuAt(source->GetWindow()->GetNativeWindow(), NULL,
                            gfx::Rect(p, gfx::Size(0,0)),
                            views::MenuItemView::TOPLEFT,
                            true);
  }
}

// BrowserView, views::MenuListener implementation.
void BrowserView::OnMenuOpened() {
  // Save the focused widget before wrench menu opens.
  saved_focused_widget_ = gtk_window_get_focus(GetNativeHandle());
}

// BrowserView, StatusAreaHost implementation.

Profile* BrowserView::GetProfile() const {
  return browser()->profile();
}

gfx::NativeWindow BrowserView::GetNativeWindow() const {
  return GetWindow()->GetNativeWindow();
}

bool BrowserView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  return true;
}

void BrowserView::ExecuteBrowserCommand(int id) const {
  browser()->ExecuteCommand(id);
}

void BrowserView::OpenButtonOptions(const views::View* button_view) {
  if (button_view == status_area_->network_view()) {
    browser()->OpenInternetOptionsDialog();
  } else if (button_view == status_area_->input_method_view()) {
    browser()->OpenLanguageOptionsDialog();
  } else {
    browser()->OpenSystemOptionsDialog();
  }
}

StatusAreaHost::ScreenMode BrowserView::GetScreenMode() const {
  return kBrowserMode;
}

StatusAreaHost::TextStyle BrowserView::GetTextStyle() const {
  ui::ThemeProvider* tp = GetThemeProvider();
  return tp->HasCustomImage(IDR_THEME_FRAME) ?
      StatusAreaHost::kWhiteHaloed : (IsOffTheRecord() ?
          StatusAreaHost::kWhitePlain : StatusAreaHost::kGrayEmbossed);
}

// BrowserView protected:

void BrowserView::GetAccessiblePanes(
    std::vector<AccessiblePaneView*>* panes) {
  ::BrowserView::GetAccessiblePanes(panes);
  panes->push_back(status_area_);
}

// BrowserView private.

void BrowserView::InitSystemMenu() {
  system_menu_.reset(new views::MenuItemView(system_menu_delegate_.get()));
  system_menu_->AppendDelegateMenuItem(IDC_RESTORE_TAB);

  system_menu_->AppendMenuItemWithLabel(
      IDC_NEW_TAB,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NEW_TAB)));
  system_menu_->AppendSeparator();
  system_menu_->AppendMenuItemWithLabel(
      IDC_TASK_MANAGER,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_TASK_MANAGER)));
}

}  // namespace chromeos

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // Create a browser view for chromeos.
  BrowserView* view;
  if (browser->is_type_popup() || browser->is_type_panel())
    view = new chromeos::PanelBrowserView(browser);
  else
    view = new chromeos::BrowserView(browser);
  (new BrowserFrame(view))->InitBrowserFrame();
  return view;
}
