// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromeos_browser_view.h"

#include "app/gfx/canvas.h"
#include "app/menus/simple_menu_model.h"
#include "app/theme_provider.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/chromeos/compact_navigation_bar.h"
#include "chrome/browser/chromeos/main_menu.h"
#include "chrome/browser/chromeos/panel_browser_view.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/chrome_browser_view_layout_manager.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/x11_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/window/hit_test.h"
#include "views/window/window.h"

namespace {

const char* kChromeOsWindowManagerName = "chromeos-wm";
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
  ChromeosTab(TabStrip* tab_strip, chromeos::ChromeosBrowserView* browser_view)
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
  chromeos::ChromeosBrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosTab);
};

// A Tabstrip that uses ChromeosTab as a Tab implementation.
class ChromeosTabStrip : public TabStrip {
 public:
  ChromeosTabStrip(TabStripModel* model,
                   chromeos::ChromeosBrowserView* browser_view)
      : TabStrip(model), browser_view_(browser_view) {
  }
  virtual ~ChromeosTabStrip() {}

 protected:
  // Overridden from TabStrip.
  virtual Tab* CreateTab() {
    return new ChromeosTab(this, browser_view_);
  }

 private:
  chromeos::ChromeosBrowserView* browser_view_;

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
};

// LayoutManager for ChromeosBrowserView, which layouts extra components such as
// main menu, stataus views.
class ChromeosBrowserViewLayoutManager : public ChromeBrowserViewLayoutManager {
 public:
  ChromeosBrowserViewLayoutManager() : ChromeBrowserViewLayoutManager() {}
  virtual ~ChromeosBrowserViewLayoutManager() {}

  //////////////////////////////////////////////////////////////////////////////
  // ChromeBrowserViewLayoutManager overrides:

  void Installed(views::View* host) {
    main_menu_ = NULL;
    compact_navigation_bar_ = NULL;
    status_area_ = NULL;
    spacer_ = NULL;
    ChromeBrowserViewLayoutManager::Installed(host);
  }

  void ViewAdded(views::View* host,
                 views::View* view) {
    ChromeBrowserViewLayoutManager::ViewAdded(host, view);
    switch (view->GetID()) {
      case VIEW_ID_SPACER:
        spacer_ = view;
        break;
      case VIEW_ID_MAIN_MENU:
        main_menu_ = view;
        break;
      case VIEW_ID_STATUS_AREA:
        status_area_ = static_cast<chromeos::StatusAreaView*>(view);
        break;
      case VIEW_ID_COMPACT_NAV_BAR:
        compact_navigation_bar_ = view;
        break;
    }
  }

  virtual int LayoutTabStrip() {
    if (!browser_view_->IsTabStripVisible()) {
      tabstrip_->SetVisible(false);
      tabstrip_->SetBounds(0, 0, 0, 0);
      return 0;
    } else {
      gfx::Rect layout_bounds =
          browser_view_->frame()->GetBoundsForTabStrip(tabstrip_);
      gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
      tabstrip_->SetBackgroundOffset(
          gfx::Point(layout_bounds.x() - toolbar_bounds.x(),
                     layout_bounds.y()));
      gfx::Point tabstrip_origin = layout_bounds.origin();
      views::View::ConvertPointToView(browser_view_->GetParent(), browser_view_,
                                      &tabstrip_origin);
      layout_bounds.set_origin(tabstrip_origin);
      tabstrip_->SetVisible(true);
      tabstrip_->SetBounds(layout_bounds);

      int bottom = 0;
      gfx::Rect tabstrip_bounds;
      LayoutCompactNavigationBar(
          layout_bounds, &tabstrip_bounds, &bottom);
      tabstrip_->SetVisible(true);
      tabstrip_->SetBounds(tabstrip_bounds);
      return bottom;
    }
  }

  virtual bool IsPositionInWindowCaption(const gfx::Point& point) {
    return ChromeBrowserViewLayoutManager::IsPositionInWindowCaption(point)
        && !IsPointInViewsInTitleArea(point);
  }

  virtual int NonClientHitTest(const gfx::Point& point) {
    gfx::Point point_in_browser_view_coords(point);
    views::View::ConvertPointToView(
        browser_view_->GetParent(), browser_view_,
        &point_in_browser_view_coords);
    if (IsPointInViewsInTitleArea(point_in_browser_view_coords)) {
      return HTCLIENT;
    }
    return ChromeBrowserViewLayoutManager::NonClientHitTest(point);
  }

 private:
  chromeos::ChromeosBrowserView* chromeos_browser_view() {
    return static_cast<chromeos::ChromeosBrowserView*>(browser_view_);
  }

  // Test if the point is on one of views that are within the
  // considered title bar area of client view.
  bool IsPointInViewsInTitleArea(const gfx::Point& point)
      const {
    gfx::Point point_in_main_menu_coords(point);
    views::View::ConvertPointToView(browser_view_, main_menu_,
                                    &point_in_main_menu_coords);
    if (main_menu_->HitTest(point_in_main_menu_coords))
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

  void LayoutCompactNavigationBar(const gfx::Rect& bounds,
                                  gfx::Rect* tabstrip_bounds,
                                  int* bottom) {
    if (browser_view_->IsTabStripVisible()) {
      *bottom = bounds.bottom();
    } else {
      *bottom = 0;
    }
    // Skip if there is no space to layout, or if the browser is in
    // fullscreen mode.
    if (bounds.IsEmpty() || browser_view_->IsFullscreen()) {
      main_menu_->SetVisible(false);
      compact_navigation_bar_->SetVisible(false);
      status_area_->SetVisible(false);
      tabstrip_bounds->SetRect(bounds.x(), bounds.y(),
                               bounds.width(), bounds.height());
      return;
    } else {
      main_menu_->SetVisible(true);
      compact_navigation_bar_->SetVisible(
          chromeos_browser_view()->is_compact_style());
      status_area_->SetVisible(true);
    }

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
    gfx::Size main_menu_size = main_menu_->GetPreferredSize();

    // TODO(oshima): Use 0 for x position for now as this is
    // sufficient for chromeos where the window is always
    // maximized. The correct value is
    // OpaqueBrowserFrameView::NonClientBorderThickness() and we will
    // consider exposing it once we settle on the UI.
    main_menu_->SetBounds(0, bounds.y(),
                          main_menu_size.width(), bounds.height());

    status_area_->Update();
    // Layout status area after tab strip.
    gfx::Size status_size = status_area_->GetPreferredSize();
    status_area_->SetBounds(bounds.x() + bounds.width() - status_size.width(),
                            bounds.y(), status_size.width(),
                            status_size.height());
    int curx = bounds.x();
    int remaining_width = bounds.width() - status_size.width();

    if (compact_navigation_bar_->IsVisible()) {
      gfx::Size cnb_bounds = compact_navigation_bar_->GetPreferredSize();
      // This (+1/-1) is a quick hack for the bug
      // http://code.google.com/p/chromium-os/issues/detail?id=1010
      // while investigating the issue. It could be in gtk or around
      // NativeViewHostGtk::CreateFixed, but it will take some time.
      compact_navigation_bar_->SetBounds(curx, bounds.y() + 1,
                                         cnb_bounds.width(),
                                         bounds.height() - 1);
      curx += cnb_bounds.width();
      remaining_width -= cnb_bounds.width();

      spacer_->SetVisible(true);
      spacer_->SetBounds(0, *bottom, browser_view_->width(),
                         kCompactNavbarSpaceHeight);
      *bottom += kCompactNavbarSpaceHeight;
    } else {
      spacer_->SetVisible(false);
    }
    // In case there is no space left.
    remaining_width = std::max(0, remaining_width);
    tabstrip_bounds->SetRect(curx, bounds.y(),
                             remaining_width, bounds.height());
  }


  views::View* main_menu_;
  chromeos::StatusAreaView* status_area_;
  views::View* compact_navigation_bar_;
  views::View* spacer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosBrowserViewLayoutManager);
};

}  // namespace

namespace chromeos {

ChromeosBrowserView::ChromeosBrowserView(Browser* browser)
    : BrowserView(browser),
      main_menu_(NULL),
      status_area_(NULL),
      compact_navigation_bar_(NULL),
      // Standard style is default.
      // TODO(oshima): Get this info from preference.
      ui_style_(StandardStyle),
      force_maximized_window_(false) {
}

////////////////////////////////////////////////////////////////////////////////
// ChromeosBrowserView, ChromeBrowserView overrides:

void ChromeosBrowserView::Init() {
  BrowserView::Init();
  main_menu_ = new views::ImageButton(this);
  main_menu_->SetID(VIEW_ID_MAIN_MENU);
  ThemeProvider* theme_provider =
      frame()->GetThemeProviderForFrame();
  SkBitmap* image = theme_provider->GetBitmapNamed(IDR_MAIN_MENU_BUTTON);
  main_menu_->SetImage(views::CustomButton::BS_NORMAL, image);
  main_menu_->SetImage(views::CustomButton::BS_HOT, image);
  main_menu_->SetImage(views::CustomButton::BS_PUSHED, image);
  AddChildView(main_menu_);

  compact_location_bar_host_.reset(
      new chromeos::CompactLocationBarHost(this));
  compact_navigation_bar_ =
      new chromeos::CompactNavigationBar(this);
  compact_navigation_bar_->SetID(VIEW_ID_COMPACT_NAV_BAR);
  AddChildView(compact_navigation_bar_);
  compact_navigation_bar_->Init();
  status_area_ = new chromeos::StatusAreaView(this);
  status_area_->SetID(VIEW_ID_STATUS_AREA);
  AddChildView(status_area_);
  status_area_->Init();

  SkBitmap* theme_toolbar = theme_provider->GetBitmapNamed(IDR_THEME_TOOLBAR);
  spacer_ = new Spacer(theme_toolbar);
  spacer_->SetID(VIEW_ID_SPACER);
  AddChildView(spacer_);

  InitSystemMenu();
  chromeos::MainMenu::ScheduleCreation();

  // The ContextMenuController has to be set to a NonClientView but
  // not to a NonClientFrameView because a TabStrip is not a child of
  // a NonClientFrameView even though visually a TabStrip is over a
  // NonClientFrameView.
  BrowserFrameGtk* gtk_frame = static_cast<BrowserFrameGtk*>(frame());
  gtk_frame->GetNonClientView()->SetContextMenuController(this);

  if (browser()->type() == Browser::TYPE_NORMAL) {
    std::string wm_name;
    bool wm_name_valid = x11_util::GetWindowManagerName(&wm_name);
    // NOTE: On Chrome OS the wm and Chrome are started in parallel. This
    // means it's possible for us not to be able to get the name of the window
    // manager. We assume that when this happens we're on Chrome OS.
    force_maximized_window_ = (!wm_name_valid ||
                               wm_name == kChromeOsWindowManagerName ||
                               CommandLine::ForCurrentProcess()->HasSwitch(
                                   switches::kChromeosFrame));
  }
}

void ChromeosBrowserView::Show() {
  bool was_visible = frame()->GetWindow()->IsVisible();
  BrowserView::Show();
  if (!was_visible) {
    TabOverviewTypes::instance()->SetWindowType(
        GTK_WIDGET(frame()->GetWindow()->GetNativeWindow()),
        TabOverviewTypes::WINDOW_TYPE_CHROME_TOPLEVEL,
        NULL);
  }
}

bool ChromeosBrowserView::IsToolbarVisible() const {
  if (is_compact_style())
    return false;
  return BrowserView::IsToolbarVisible();
}

void ChromeosBrowserView::SetFocusToLocationBar() {
  if (compact_navigation_bar_->IsFocusable()) {
    compact_navigation_bar_->FocusLocation();
  } else {
    BrowserView::SetFocusToLocationBar();
  }
}

void ChromeosBrowserView::ToggleCompactNavigationBar() {
  ui_style_ = static_cast<UIStyle>((ui_style_ + 1) % 2);
  compact_navigation_bar_->SetFocusable(is_compact_style());
  compact_location_bar_host_->SetEnabled(is_compact_style());
  Layout();
}

views::LayoutManager* ChromeosBrowserView::CreateLayoutManager() const {
  return new ChromeosBrowserViewLayoutManager();
}

TabStrip* ChromeosBrowserView::CreateTabStrip(
    TabStripModel* tab_strip_model) {
  return new ChromeosTabStrip(tab_strip_model, this);
}

// views::ButtonListener overrides.
void ChromeosBrowserView::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  chromeos::MainMenu::Show(browser());
}

// views::ContextMenuController overrides.
void ChromeosBrowserView::ShowContextMenu(views::View* source,
                                          int x,
                                          int y,
                                          bool is_mouse_gesture) {
  system_menu_menu_->RunMenuAt(gfx::Point(x, y), views::Menu2::ALIGN_TOPLEFT);
}

////////////////////////////////////////////////////////////////////////////////
// ChromeosBrowserView public:

void ChromeosBrowserView::ShowCompactLocationBarUnderSelectedTab() {
  if (!is_compact_style())
    return;
  int index = browser()->selected_index();
  compact_location_bar_host_->Update(index, true);
}

bool ChromeosBrowserView::ShouldForceMaximizedWindow() const {
  return force_maximized_window_;
}

int ChromeosBrowserView::GetMainMenuWidth() const {
  return main_menu_->GetPreferredSize().width();
}

////////////////////////////////////////////////////////////////////////////////
// ChromeosBrowserView private:

void ChromeosBrowserView::InitSystemMenu() {
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
    view = new chromeos::ChromeosBrowserView(browser);
  BrowserFrame::Create(view, browser->profile());
  return view;
}
