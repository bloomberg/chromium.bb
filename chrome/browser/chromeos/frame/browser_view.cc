// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_view.h"

#include <algorithm>
#include <string>

#include "app/gfx/canvas.h"
#include "app/menus/simple_menu_model.h"
#include "app/theme_provider.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/app_launcher.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/chromeos/compact_navigation_bar.h"
#include "chrome/browser/chromeos/frame/panel_browser_view.h"
#include "chrome/browser/chromeos/status/browser_status_area_view.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/browser_view_layout.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/window/hit_test.h"
#include "views/window/window.h"

namespace {

// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;

// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;

// The size of the space between the content area and the tabstrip that is
// inserted in compact nvaigation bar mode.
const int kCompactNavbarSpaceHeight = 3;

// A space we insert between the tabstrip and the content in
// Compact mode.
class Spacer : public views::View {
  SkBitmap* background_;
 public:
  explicit Spacer(SkBitmap* bitmap) : background_(bitmap) {}

  void Paint(gfx::Canvas* canvas) {
    canvas->TileImageInt(*background_, 0, 0, width(), height());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Spacer);
};

// A chromeos implementation of Tab that shows the compact location bar.
class ChromeosTab : public Tab {
 public:
  ChromeosTab(TabStrip* tab_strip, chromeos::BrowserView* browser_view)
      : Tab(tab_strip),
        browser_view_(browser_view) {
  }
  virtual ~ChromeosTab() {}

  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) {
    TabRenderer::OnMouseEntered(event);
    browser_view_->ShowCompactLocationBarUnderSelectedTab();
  }

 private:
  chromeos::BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosTab);
};

// A Tabstrip that uses ChromeosTab as a Tab implementation.
class ChromeosTabStrip : public TabStrip {
 public:
  ChromeosTabStrip(TabStripModel* model, chromeos::BrowserView* browser_view)
      : TabStrip(model), browser_view_(browser_view) {
  }
  virtual ~ChromeosTabStrip() {}

 protected:
  // Overridden from TabStrip.
  virtual Tab* CreateTab() {
    return new ChromeosTab(this, browser_view_);
  }

 private:
  chromeos::BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosTabStrip);
};

// View ID used only in ChromeOS.
enum ChromeOSViewIds {
  // Start with the offset that is big enough to avoid possible
  // collison.
  VIEW_ID_MAIN_MENU = VIEW_ID_PREDEFINED_COUNT + 10000,
  VIEW_ID_COMPACT_NAV_BAR,
  VIEW_ID_STATUS_AREA,
  VIEW_ID_SPACER,
  VIEW_ID_OTR_AVATAR,
};

}  // namespace

namespace chromeos {

// LayoutManager for BrowserView, which layouts extra components such as
// main menu, stataus views as follows:
//                     ____  __ __
//      [AppLauncher] /    \   \  \     [StatusArea]
//
class BrowserViewLayout : public ::BrowserViewLayout {
 public:
  BrowserViewLayout() : ::BrowserViewLayout() {}
  virtual ~BrowserViewLayout() {}

  //////////////////////////////////////////////////////////////////////////////
  // BrowserViewLayout overrides:

  void Installed(views::View* host) {
    main_menu_button_ = NULL;
    compact_navigation_bar_ = NULL;
    status_area_ = NULL;
    spacer_ = NULL;
    ::BrowserViewLayout::Installed(host);
  }

  void ViewAdded(views::View* host,
                 views::View* view) {
    ::BrowserViewLayout::ViewAdded(host, view);
    switch (view->GetID()) {
      case VIEW_ID_SPACER:
        spacer_ = view;
        break;
      case VIEW_ID_MAIN_MENU:
        main_menu_button_ = view;
        break;
      case VIEW_ID_STATUS_AREA:
        status_area_ = static_cast<chromeos::StatusAreaView*>(view);
        break;
      case VIEW_ID_COMPACT_NAV_BAR:
        compact_navigation_bar_ = view;
        break;
      case VIEW_ID_OTR_AVATAR:
        otr_avatar_icon_ = view;
        break;
    }
  }

  // In the normal and the compact navigation bar mode, ChromeOS
  // layouts compact navigation buttons and status views in the title
  // area. See Layout
  virtual int LayoutTabStrip() {
    if (browser_view_->IsFullscreen() ||
        !browser_view_->IsTabStripVisible()) {
      main_menu_button_->SetVisible(false);
      compact_navigation_bar_->SetVisible(false);
      status_area_->SetVisible(false);
      otr_avatar_icon_->SetVisible(false);
      tabstrip_->SetVisible(false);
      tabstrip_->SetBounds(0, 0, 0, 0);
      return 0;
    } else {
      gfx::Rect layout_bounds =
          browser_view_->frame()->GetBoundsForTabStrip(tabstrip_);
      gfx::Point tabstrip_origin = layout_bounds.origin();
      views::View::ConvertPointToView(browser_view_->GetParent(), browser_view_,
                                      &tabstrip_origin);
      layout_bounds.set_origin(tabstrip_origin);
      return LayoutTitlebarComponents(layout_bounds);
    }
  }

  virtual bool IsPositionInWindowCaption(const gfx::Point& point) {
    return ::BrowserViewLayout::IsPositionInWindowCaption(point)
        && !IsPointInViewsInTitleArea(point);
  }

  virtual int NonClientHitTest(const gfx::Point& point) {
    gfx::Point point_in_browser_view_coords(point);
    views::View::ConvertPointToView(
        browser_view_->GetParent(), browser_view_,
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
    gfx::Point point_in_main_menu_coords(point);
    views::View::ConvertPointToView(browser_view_, main_menu_button_,
                                    &point_in_main_menu_coords);
    if (main_menu_button_->HitTest(point_in_main_menu_coords))
      return true;

    gfx::Point point_in_status_area_coords(point);
    views::View::ConvertPointToView(browser_view_, status_area_,
                                    &point_in_status_area_coords);
    if (status_area_->HitTest(point_in_status_area_coords))
      return true;

    if (compact_navigation_bar_->IsVisible()) {
      gfx::Point point_in_cnb_coords(point);
      views::View::ConvertPointToView(browser_view_,
                                      compact_navigation_bar_,
                                      &point_in_cnb_coords);
      return compact_navigation_bar_->HitTest(point_in_cnb_coords);
    }
    return false;
  }

  // Layouts components in the title bar area (given by
  // |bounds|). These include the main menu, the compact navigation
  // buttons (in compact navbar mode), the otr avatar icon (in
  // incognito window), the tabstrip and the the status area.
  int LayoutTitlebarComponents(const gfx::Rect& bounds) {
    if (bounds.IsEmpty()) {
      return 0;
    }
    main_menu_button_->SetVisible(true);
    compact_navigation_bar_->SetVisible(
        chromeos_browser_view()->is_compact_style());
    tabstrip_->SetVisible(true);
    otr_avatar_icon_->SetVisible(browser_view_->ShouldShowOffTheRecordAvatar());
    status_area_->SetVisible(true);

    int bottom = bounds.bottom();

    /* TODO(oshima):
     * Disabling the ability to update location bar on re-layout bacause
     * tabstrip state may not be in sync with the browser's state when
     * new tab is added. We should decide when we know more about this
     * feature. May be we should simply hide the location?
     * Filed a bug: http://crbug.com/30612.
     if (compact_navigation_bar_->IsVisible()) {
     // Update the size and location of the compact location bar.
     int index = browser_view()->browser()->selected_index();
     compact_location_bar_host_->Update(index, false);
     }
    */

    // Layout main menu before tab strip.
    gfx::Size main_menu_size = main_menu_button_->GetPreferredSize();
    main_menu_button_->SetBounds(0, bounds.y(),
                                 main_menu_size.width(), bounds.height());

    status_area_->Update();
    // Layout status area after tab strip.
    gfx::Size status_size = status_area_->GetPreferredSize();
    status_area_->SetBounds(bounds.x() + bounds.width() - status_size.width(),
                            bounds.y(), status_size.width(),
                            status_size.height());
    LayoutOTRAvatar(bounds);

    int curx = bounds.x() + main_menu_size.width();

    if (compact_navigation_bar_->IsVisible()) {
      gfx::Size cnb_size = compact_navigation_bar_->GetPreferredSize();
      // Adjust the size of the compact nativation bar to avoid creating
      // a fixed widget with its own gdk window. AutocompleteEditView
      // expects the parent view to be transparent, but a fixed with
      // its own window is not.
      gfx::Rect cnb_bounds(curx, bounds.y(), cnb_size.width(), bounds.height());
      compact_navigation_bar_->SetBounds(
          cnb_bounds.Intersect(browser_view_->GetVisibleBounds()));
      curx += cnb_bounds.width();

      spacer_->SetVisible(true);
      spacer_->SetBounds(0, bottom, browser_view_->width(),
                         kCompactNavbarSpaceHeight);
      bottom += kCompactNavbarSpaceHeight;
    } else {
      compact_navigation_bar_->SetBounds(curx, bounds.y(), 0, 0);
      spacer_->SetVisible(false);
    }
    int remaining_width = std::max(
        0,  // In case there is no space left.
        otr_avatar_icon_->bounds().x() -
        compact_navigation_bar_->bounds().right());
    tabstrip_->SetBounds(curx, bounds.y(), remaining_width, bounds.height());

    gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
    tabstrip_->SetBackgroundOffset(
        gfx::Point(curx - toolbar_bounds.x(),
                   bounds.y()));
    return bottom;
  }

  // Layouts OTR avatar within the given |bounds|.
  void LayoutOTRAvatar(const gfx::Rect& bounds) {
    gfx::Rect status_bounds = status_area_->bounds();
    if (!otr_avatar_icon_->IsVisible()) {
      otr_avatar_icon_->SetBounds(status_bounds.x(), status_bounds.y(), 0, 0);
    } else {
      gfx::Size preferred_size = otr_avatar_icon_->GetPreferredSize();

      int y = bounds.bottom() - preferred_size.height() - kOTRBottomSpacing;
      int x = status_bounds.x() - kOTRSideSpacing - preferred_size.width();
      otr_avatar_icon_->SetBounds(x, y, preferred_size.width(),
                                  preferred_size.height());
    }
  }


  views::View* main_menu_button_;
  chromeos::StatusAreaView* status_area_;
  views::View* compact_navigation_bar_;
  views::View* spacer_;
  views::View* otr_avatar_icon_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

BrowserView::BrowserView(Browser* browser)
    : ::BrowserView(browser),
      main_menu_(NULL),
      main_menu_button_(NULL),
      status_area_(NULL),
      compact_navigation_bar_(NULL),
      // Standard style is default.
      // TODO(oshima): Get this info from preference.
      ui_style_(StandardStyle),
      force_maximized_window_(false),
      spacer_(NULL),
      otr_avatar_icon_(new views::ImageView()) {
}

BrowserView::~BrowserView() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserView, ::BrowserView overrides:

void BrowserView::Init() {
  ::BrowserView::Init();
  main_menu_button_ = new views::ImageButton(this);
  main_menu_button_->SetID(VIEW_ID_MAIN_MENU);
  ThemeProvider* theme_provider =
      frame()->GetThemeProviderForFrame();
  SkBitmap* image = theme_provider->GetBitmapNamed(IDR_APP_LAUNCHER_BUTTON);
  main_menu_button_->SetImage(views::CustomButton::BS_NORMAL, image);
  main_menu_button_->SetImage(views::CustomButton::BS_HOT, image);
  main_menu_button_->SetImage(views::CustomButton::BS_PUSHED, image);
  AddChildView(main_menu_button_);

  compact_location_bar_host_.reset(
      new chromeos::CompactLocationBarHost(this));
  compact_navigation_bar_ =
      new chromeos::CompactNavigationBar(this);
  compact_navigation_bar_->SetID(VIEW_ID_COMPACT_NAV_BAR);
  AddChildView(compact_navigation_bar_);
  compact_navigation_bar_->Init();
  status_area_ = new BrowserStatusAreaView(this);
  status_area_->SetID(VIEW_ID_STATUS_AREA);
  AddChildView(status_area_);
  status_area_->Init();
  ToolbarView* toolbar_view = GetToolbarView();
  toolbar_view->SetAppMenuModel(status_area_->CreateAppMenuModel(toolbar_view));

  SkBitmap* theme_toolbar = theme_provider->GetBitmapNamed(IDR_THEME_TOOLBAR);
  spacer_ = new Spacer(theme_toolbar);
  spacer_->SetID(VIEW_ID_SPACER);
  AddChildView(spacer_);

  InitSystemMenu();

  // The ContextMenuController has to be set to a NonClientView but
  // not to a NonClientFrameView because a TabStrip is not a child of
  // a NonClientFrameView even though visually a TabStrip is over a
  // NonClientFrameView.
  BrowserFrameGtk* gtk_frame = static_cast<BrowserFrameGtk*>(frame());
  gtk_frame->GetNonClientView()->SetContextMenuController(this);

  otr_avatar_icon_->SetImage(GetOTRAvatarIcon());
  otr_avatar_icon_->SetID(VIEW_ID_OTR_AVATAR);
  AddChildView(otr_avatar_icon_);
}

void BrowserView::Show() {
  bool was_visible = frame()->GetWindow()->IsVisible();
  ::BrowserView::Show();
  if (!was_visible) {
    WmIpc::instance()->SetWindowType(
        GTK_WIDGET(frame()->GetWindow()->GetNativeWindow()),
        WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL,
        NULL);
  }
}

bool BrowserView::IsToolbarVisible() const {
  if (is_compact_style())
    return false;
  return ::BrowserView::IsToolbarVisible();
}

void BrowserView::SetFocusToLocationBar() {
  if (is_compact_style()) {
    ShowCompactLocationBarUnderSelectedTab();
  } else {
    ::BrowserView::SetFocusToLocationBar();
  }
}

void BrowserView::ToggleCompactNavigationBar() {
  ui_style_ = static_cast<UIStyle>((ui_style_ + 1) % 2);
  compact_location_bar_host_->SetEnabled(is_compact_style());
  compact_location_bar_host_->Hide(false);
  Layout();
}

views::LayoutManager* BrowserView::CreateLayoutManager() const {
  return new BrowserViewLayout();
}

BaseTabStrip* BrowserView::CreateTabStrip(
    TabStripModel* tab_strip_model) {
  return new ChromeosTabStrip(tab_strip_model, this);
}

void BrowserView::SetStarredState(bool is_starred) {
  ::BrowserView::SetStarredState(is_starred);
  compact_location_bar_host_->GetStarButton()->SetToggled(is_starred);
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  if (is_compact_style())
    compact_location_bar_host_->GetStarButton()->ShowStarBubble(
        url, !already_bookmarked);
  else
    ::BrowserView::ShowBookmarkBubble(url, already_bookmarked);
}

// views::ButtonListener overrides.
void BrowserView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (!main_menu_.get()) {
    main_menu_.reset(new AppLauncher());
  }
  main_menu_->Show(browser());
}

// views::ContextMenuController overrides.
void BrowserView::ShowContextMenu(views::View* source,
                                  const gfx::Point& p,
                                  bool is_mouse_gesture) {
  // Only show context menu if point is in unobscured parts of browser, i.e.
  // if NonClientHitTest returns :
  // - HTCAPTION: in title bar or unobscured part of tabstrip
  // - HTNOWHERE: as the name implies.
  gfx::Point point_in_parent_coords(p);
  views::View::ConvertPointToView(NULL, GetParent(), &point_in_parent_coords);
  int hit_test = NonClientHitTest(point_in_parent_coords);
  if (hit_test == HTCAPTION || hit_test == HTNOWHERE)
    system_menu_menu_->RunMenuAt(p, views::Menu2::ALIGN_TOPLEFT);
}

// StatusAreaHost overrides.
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

void BrowserView::OpenButtonOptions(const views::View* button_view) const {
  if (button_view == status_area_->network_view())
    browser()->OpenInternetOptionsDialog();
  else
    browser()->OpenSystemOptionsDialog();
}

bool BrowserView::IsButtonVisible(const views::View* button_view) const {
  if (button_view == status_area_->menu_view())
    return !IsToolbarVisible();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserView public:

void BrowserView::ShowCompactLocationBarUnderSelectedTab() {
  if (!is_compact_style())
    return;
  int index = browser()->selected_index();
  compact_location_bar_host_->Update(index, true);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserView private:

void BrowserView::InitSystemMenu() {
  system_menu_contents_.reset(new menus::SimpleMenuModel(this));
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB,
                                               IDS_RESTORE_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                               IDS_TASK_MANAGER);
  system_menu_menu_.reset(new views::Menu2(system_menu_contents_.get()));
}

}  // namespace chromeos

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // Create a browser view for chromeos.
  BrowserView* view;
  if (browser->type() & Browser::TYPE_POPUP)
    view = new chromeos::PanelBrowserView(browser);
  else
    view = new chromeos::BrowserView(browser);
  BrowserFrame::Create(view, browser->profile());
  return view;
}
