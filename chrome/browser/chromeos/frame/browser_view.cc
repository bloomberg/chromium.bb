// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_view.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/frame/layout_mode_button.h"
#include "chrome/browser/chromeos/frame/panel_browser_view.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/theme_background.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_runner.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

namespace {

// Amount to tweak the position of the status area to get it to look right.
const int kStatusAreaVerticalAdjustment = -1;

// If a popup window is larger than this fraction of the screen, create a tab.
const float kPopupMaxWidthFactor = 0.5;
const float kPopupMaxHeightFactor = 0.6;

// GDK representation of the _CHROME_STATE X atom.
static GdkAtom g_chrome_state_gdk_atom = 0;

// This MenuItemView delegate class forwards to the
// SimpleMenuModel::Delegate() implementation.
class SimpleMenuModelDelegateAdapter : public views::MenuDelegate {
 public:
  explicit SimpleMenuModelDelegateAdapter(
      ui::SimpleMenuModel::Delegate* simple_menu_model_delegate);

  // views::MenuDelegate implementation.
  virtual bool GetAccelerator(int id,
                              ui::Accelerator* accelerator) OVERRIDE;
  virtual string16 GetLabel(int id) const OVERRIDE;
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
    ui::Accelerator* accelerator) {
  return simple_menu_model_delegate_->GetAcceleratorForCommandId(
      id, accelerator);
}

string16 SimpleMenuModelDelegateAdapter::GetLabel(int id) const {
  return simple_menu_model_delegate_->GetLabelForCommandId(id);
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

// LayoutManager for BrowserView, which lays out extra components such as
// the status views as follows:
//       ____  __ __
//      /    \   \  \     [StatusArea] [LayoutModeButton]
//
class BrowserViewLayout : public ::BrowserViewLayout {
 public:
  BrowserViewLayout() : ::BrowserViewLayout() {}
  virtual ~BrowserViewLayout() {}

  //////////////////////////////////////////////////////////////////////////////
  // BrowserViewLayout overrides:

  void Installed(views::View* host) {
    status_area_ = NULL;
    layout_mode_button_ = NULL;
    ::BrowserViewLayout::Installed(host);
  }

  void ViewAdded(views::View* host,
                 views::View* view) {
    ::BrowserViewLayout::ViewAdded(host, view);
    switch (view->id()) {
      case VIEW_ID_STATUS_AREA:
        status_area_ = static_cast<chromeos::StatusAreaViewChromeos*>(view);
        break;
      case VIEW_ID_LAYOUT_MODE_BUTTON:
        layout_mode_button_ = static_cast<chromeos::LayoutModeButton*>(view);
        break;
    }
  }

  // In the normal and the compact navigation bar mode, ChromeOS
  // lays out compact navigation buttons and status views in the title
  // area. See Layout
  virtual int LayoutTabStripRegion() OVERRIDE {
    if (browser_view_->IsFullscreen() || !browser_view_->IsTabStripVisible()) {
      if (status_area_) {
        status_area_->SetVisible(false);
        UpdateStatusAreaBoundsProperty();
      }
      tabstrip_->SetVisible(false);
      tabstrip_->SetBounds(0, 0, 0, 0);
      layout_mode_button_->SetVisible(false);
      layout_mode_button_->SetBounds(0, 0, 0, 0);
      return 0;
    }

    gfx::Rect tabstrip_bounds(
        browser_view_->frame()->GetBoundsForTabStrip(tabstrip_));
    gfx::Point tabstrip_origin = tabstrip_bounds.origin();
    views::View::ConvertPointToView(browser_view_->parent(), browser_view_,
                                    &tabstrip_origin);
    tabstrip_bounds.set_origin(tabstrip_origin);
    return LayoutTitlebarComponents(tabstrip_bounds);
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
    if (status_area_) {
      gfx::Point point_in_status_area_coords(point);
      views::View::ConvertPointToView(browser_view_, status_area_,
                                      &point_in_status_area_coords);
      if (status_area_->HitTest(point_in_status_area_coords))
        return true;
    }
    gfx::Point point_in_layout_mode_button_coords(point);
    views::View::ConvertPointToView(browser_view_, layout_mode_button_,
                                    &point_in_layout_mode_button_coords);
    if (layout_mode_button_->HitTest(point_in_layout_mode_button_coords))
      return true;

    return false;
  }

  // Lays out tabstrip, status area, and layout mode button in the title bar
  // area (given by |bounds|).
  int LayoutTitlebarComponents(const gfx::Rect& bounds) {
    if (bounds.IsEmpty())
      return 0;

    const bool show_layout_mode_button =
        chromeos_browser_view()->should_show_layout_mode_button();

    tabstrip_->SetVisible(true);
    if (status_area_) {
      status_area_->SetVisible(
          !chromeos_browser_view()->has_hide_status_area_property());
    }
    layout_mode_button_->SetVisible(show_layout_mode_button);

    const gfx::Size layout_mode_button_size =
        layout_mode_button_->GetPreferredSize();
    layout_mode_button_->SetBounds(
        bounds.right() - layout_mode_button_size.width(),
        bounds.y(),
        layout_mode_button_size.width(),
        layout_mode_button_size.height());

    if (status_area_) {
      // Lay out status area after tab strip and before layout mode button (if
      // shown).
      gfx::Size status_size = status_area_->GetPreferredSize();
      const int status_right =
          show_layout_mode_button ?
          layout_mode_button_->bounds().x() :
          bounds.right();
      status_area_->SetBounds(
          status_right - status_size.width(),
          bounds.y() + kStatusAreaVerticalAdjustment,
          status_size.width(),
          status_size.height());
      UpdateStatusAreaBoundsProperty();
    }
    tabstrip_->SetBounds(bounds.x(), bounds.y(),
                         std::max(0, status_area_->bounds().x() - bounds.x()),
                         bounds.height());
    return bounds.bottom();
  }

  // Updates |status_area_bounds_for_property_| based on the current bounds and
  // calls WmIpc::SetStatusBoundsProperty() if it changed.
  void UpdateStatusAreaBoundsProperty() {
    if (!status_area_)
      return;
    gfx::Rect current_bounds;
    if (status_area_->IsVisible()) {
      gfx::Rect translated_bounds =
          status_area_->parent()->ConvertRectToWidget(status_area_->bounds());
      // To avoid a dead zone across the top of the screen,
      // StatusAreaButton::HitTest() accepts clicks in the area between the top
      // of its own bounds and the top of its parent view.  Make the bounds that
      // we report match.
      current_bounds.SetRect(
          translated_bounds.x(),
          translated_bounds.y() - status_area_->bounds().y(),
          translated_bounds.width(),
          translated_bounds.height() + status_area_->bounds().y());
    }

    if (status_area_bounds_for_property_ != current_bounds) {
      status_area_bounds_for_property_ = current_bounds;
#if defined(TOOLKIT_USES_GTK)
      WmIpc::instance()->SetStatusBoundsProperty(
          GTK_WIDGET(chromeos_browser_view()->frame()->GetNativeWindow()),
          status_area_bounds_for_property_);
#endif
    }
  }

  chromeos::StatusAreaViewChromeos* status_area_;
  chromeos::LayoutModeButton* layout_mode_button_;

  // Most-recently-set bounds for the _CHROME_STATUS_BOUNDS property.
  // Empty if |status_area_| isn't visible.  Tracked here so we don't update the
  // property needlessly on no-op relayouts.
  gfx::Rect status_area_bounds_for_property_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

// BrowserView

BrowserView::BrowserView(Browser* browser)
    : ::BrowserView(browser),
      status_area_(NULL),
      layout_mode_button_(NULL),
      saved_focused_widget_(NULL),
      has_hide_status_area_property_(false),
      should_show_layout_mode_button_(false) {
  system_menu_delegate_.reset(new SimpleMenuModelDelegateAdapter(this));
  BrowserList::AddObserver(this);
  MessageLoopForUI::current()->AddObserver(this);

#if defined(TOOLKIT_USES_GTK)
  if (!g_chrome_state_gdk_atom)
    g_chrome_state_gdk_atom =
        gdk_atom_intern(
            WmIpc::instance()->GetAtomName(WmIpc::ATOM_CHROME_STATE).c_str(),
            FALSE);  // !only_if_exists
#endif
}

BrowserView::~BrowserView() {
  if (toolbar())
    toolbar()->RemoveMenuListener(this);
  MessageLoopForUI::current()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
}

void BrowserView::AddTrayButton(StatusAreaButton* button, bool bordered) {
  status_area_->AddButton(button, bordered);
}

void BrowserView::RemoveTrayButton(StatusAreaButton* button) {
  status_area_->RemoveButton(button);
}

bool BrowserView::ContainsButton(StatusAreaButton* button) {
  return status_area_->Contains(button);
}

chromeos::BrowserView* BrowserView::GetBrowserViewForBrowser(Browser* browser) {
  // This calls the static method BrowserView::GetBrowserViewForBrowser in the
  // global namespace. Check the chrome/browser/ui/views/frame/browser_view.h
  // file for details.
  return static_cast<chromeos::BrowserView*>(
      ::BrowserView::GetBrowserViewForBrowser(browser));
}

// BrowserView, ::BrowserView overrides:

void BrowserView::Init() {
  ::BrowserView::Init();
  status_area_ = new StatusAreaViewChromeos();
  status_area_->Init(this, StatusAreaViewChromeos::BROWSER_MODE);
  AddChildView(status_area_);

  layout_mode_button_ = new LayoutModeButton();
  AddChildView(layout_mode_button_);
  layout_mode_button_->Init();

  frame()->non_client_view()->set_context_menu_controller(this);

  // Listen to wrench menu opens.
  if (toolbar())
    toolbar()->AddMenuListener(this);

  // Listen for PropertyChange events (which we receive in DidProcessEvent()).
  gtk_widget_add_events(GTK_WIDGET(frame()->GetNativeWindow()),
                        GDK_PROPERTY_CHANGE_MASK);
  FetchHideStatusAreaProperty();
  UpdateLayoutModeButtonVisibility();

  // Make sure the window is set to the right type.
  std::vector<int> params;
  params.push_back(browser()->tab_count());
  params.push_back(browser()->active_index());
  params.push_back(gtk_get_current_event_time());
#if defined(TOOLKIT_USES_GTK)
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(frame()->GetNativeWindow()),
      WM_IPC_WINDOW_CHROME_TOPLEVEL,
      &params);
#endif
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
#if defined(TOOLKIT_USES_GTK)
    WmIpc::instance()->SetWindowType(
        GTK_WIDGET(frame()->GetNativeWindow()),
        WM_IPC_WINDOW_CHROME_TOPLEVEL,
        &params);
#endif
  }
}

void BrowserView::FocusChromeOSStatus() {
  if (status_area_)
    status_area_->SetPaneFocus(NULL);
}

views::LayoutManager* BrowserView::CreateLayoutManager() const {
  return new BrowserViewLayout();
}

void BrowserView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

bool BrowserView::GetSavedWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  if (system::runtime_environment::IsRunningOnChromeOS() ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kStartMaximized)) {
    // Typically we don't request a full screen size. This means we'll request a
    // non-full screen size, layout/paint at that size, then the window manager
    // will snap us to full screen size. This results in an ugly
    // resize/paint. To avoid this we always request a full screen size.
    *bounds = GetWidget()->GetWorkAreaBoundsInScreen();
    *show_state = ui::SHOW_STATE_NORMAL;
    return true;
  }
  return ::BrowserView::GetSavedWindowPlacement(bounds, show_state);
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

// This is a static function so that the logic can be shared by
// chromeos::PanelBrowserView.
WindowOpenDisposition BrowserView::DispositionForPopupBounds(
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

WindowOpenDisposition BrowserView::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return DispositionForPopupBounds(bounds);
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
    if (system_menu_runner_->RunMenuAt(source->GetWidget(), NULL,
            gfx::Rect(p, gfx::Size(0,0)), views::MenuItemView::TOPLEFT,
            views::MenuRunner::HAS_MNEMONICS) ==
        views::MenuRunner::MENU_DELETED)
      return;
  }
}

// BrowserView, views::MenuListener implementation.
void BrowserView::OnMenuOpened() {
  // Save the focused widget before wrench menu opens.
  saved_focused_widget_ = gtk_window_get_focus(GetNativeHandle());
}

// BrowserView, BrowserList::Observer implementation.

void BrowserView::OnBrowserAdded(const Browser* browser) {
  const bool was_showing = should_show_layout_mode_button_;
  UpdateLayoutModeButtonVisibility();
  if (should_show_layout_mode_button_ != was_showing)
    Layout();
}

void BrowserView::OnBrowserRemoved(const Browser* browser) {
  const bool was_showing = should_show_layout_mode_button_;
  UpdateLayoutModeButtonVisibility();
  if (should_show_layout_mode_button_ != was_showing)
    Layout();
}

// StatusAreaButton::Delegate overrides.

bool BrowserView::ShouldExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) const {
  return true;
}

void BrowserView::ExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) {
  switch (command_id) {
    case StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS:
      browser()->OpenInternetOptionsDialog();
      break;
    case StatusAreaButton::Delegate::SHOW_LANGUAGE_OPTIONS:
      browser()->OpenLanguageOptionsDialog();
      break;
    case StatusAreaButton::Delegate::SHOW_SYSTEM_OPTIONS:
      browser()->OpenSystemOptionsDialog();
      break;
    default:
      NOTREACHED();
  }
}

gfx::Font BrowserView::GetStatusAreaFont(const gfx::Font& font) const {
  return font.DeriveFont(0, gfx::Font::BOLD);
}

StatusAreaButton::TextStyle BrowserView::GetStatusAreaTextStyle() const {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());

  if (!theme_service->UsingDefaultTheme())
    return StatusAreaButton::WHITE_HALOED;

  return IsOffTheRecord() ?
      StatusAreaButton::WHITE_PLAIN : StatusAreaButton::GRAY_EMBOSSED;
}

void BrowserView::ButtonVisibilityChanged(views::View* button_view) {
  if (status_area_)
    status_area_->UpdateButtonVisibility();
}

// BrowserView, MessageLoopForUI::Observer implementation.

#if defined(TOUCH_UI) || defined(USE_AURA)
base::EventStatus BrowserView::WillProcessEvent(
    const base::NativeEvent& event) OVERRIDE {
  return base::EVENT_CONTINUE;
}

void BrowserView::DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  // TODO(oshima): On Aura, WM should notify chrome someshow.
}
#else
void BrowserView::DidProcessEvent(GdkEvent* event) {
  if (event->type == GDK_PROPERTY_NOTIFY) {
    if (!frame()->GetNativeWindow())
      return;

    GdkEventProperty* property_event =
        reinterpret_cast<GdkEventProperty*>(event);
    if (property_event->window ==
        GTK_WIDGET(frame()->GetNativeWindow())->window) {
      if (property_event->atom == g_chrome_state_gdk_atom) {
        const bool had_property = has_hide_status_area_property_;
        FetchHideStatusAreaProperty();
        if (has_hide_status_area_property_ != had_property)
          Layout();
      }
    }
  }
}
#endif

// BrowserView protected:

void BrowserView::GetAccessiblePanes(
    std::vector<views::AccessiblePaneView*>* panes) {
  ::BrowserView::GetAccessiblePanes(panes);
  if (status_area_)
    panes->push_back(status_area_);
}

// BrowserView private.

void BrowserView::InitSystemMenu() {
  views::MenuItemView* menu =
      new views::MenuItemView(system_menu_delegate_.get());
  // MenuRunner takes ownership of menu.
  system_menu_runner_.reset(new views::MenuRunner(menu));
  menu->AppendDelegateMenuItem(IDC_RESTORE_TAB);
  menu->AppendMenuItemWithLabel(IDC_NEW_TAB,
                                l10n_util::GetStringUTF16(IDS_NEW_TAB));
  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_TASK_MANAGER,
                                l10n_util::GetStringUTF16(IDS_TASK_MANAGER));
}

void BrowserView::FetchHideStatusAreaProperty() {
#if defined(TOOLKIT_USES_GTK)
  std::set<WmIpc::AtomType> state_atoms;
  if (WmIpc::instance()->GetWindowState(
          GTK_WIDGET(frame()->GetNativeWindow()), &state_atoms)) {
    if (state_atoms.count(WmIpc::ATOM_CHROME_STATE_STATUS_HIDDEN)) {
      has_hide_status_area_property_ = true;
      return;
    }
  }
#endif
  has_hide_status_area_property_ = false;
}

void BrowserView::UpdateLayoutModeButtonVisibility() {
  int count = 0;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->is_type_tabbed()) {
      ++count;
      if (count >= 2) {
        should_show_layout_mode_button_ = true;
        return;
      }
    }
  }
  should_show_layout_mode_button_ = false;
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
