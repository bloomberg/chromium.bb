// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/toolbar_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/wrench_menu.h"
#include "chrome/browser/wrench_menu_model.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/canvas_skia.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "gfx/skbitmap_operations.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button_dropdown.h"
#include "views/focus/view_storage.h"
#include "views/widget/tooltip_manager.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kControlHorizOffset = 4;
static const int kControlVertOffset = 6;
static const int kControlIndent = 3;
static const int kStatusBubbleWidth = 480;

// The length of time to run the upgrade notification animation (the time it
// takes one pulse to run its course and go back to its original brightness).
static const int kPulseDuration = 2000;

// How long to wait between pulsating the upgrade notifier.
static const int kPulsateEveryMs = 8000;

// The offset in pixels of the upgrade dot on the app menu.
static const int kUpgradeDotOffset = 11;

// Separation between the location bar and the menus.
static const int kMenuButtonOffset = 3;

// Padding to the right of the location bar
static const int kPaddingRight = 2;

static const int kPopupTopSpacingNonGlass = 3;
static const int kPopupBottomSpacingNonGlass = 2;
static const int kPopupBottomSpacingGlass = 1;

static SkBitmap* kPopupBackgroundEdge = NULL;

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser)
    : model_(browser->toolbar_model()),
      back_(NULL),
      forward_(NULL),
      home_(NULL),
      reload_(NULL),
      location_bar_(NULL),
      browser_actions_(NULL),
      app_menu_(NULL),
      profile_(NULL),
      browser_(browser),
      profiles_menu_contents_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      destroyed_flag_(NULL) {
  SetID(VIEW_ID_TOOLBAR);

  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);

  display_mode_ = browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ?
      DISPLAYMODE_NORMAL : DISPLAYMODE_LOCATION;

  if (!kPopupBackgroundEdge) {
    kPopupBackgroundEdge = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_LOCATIONBG_POPUPMODE_EDGE);
  }

  if (!Singleton<UpgradeDetector>::get()->notify_upgrade()) {
    registrar_.Add(this, NotificationType::UPGRADE_RECOMMENDED,
                   NotificationService::AllSources());
  }
}

ToolbarView::~ToolbarView() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;

  // NOTE: Don't remove the command observers here.  This object gets destroyed
  // after the Browser (which owns the CommandUpdater), so the CommandUpdater is
  // already gone.
}

void ToolbarView::Init(Profile* profile) {
  back_menu_model_.reset(new BackForwardMenuModel(
      browser_, BackForwardMenuModel::BACKWARD_MENU));
  forward_menu_model_.reset(new BackForwardMenuModel(
      browser_, BackForwardMenuModel::FORWARD_MENU));
  app_menu_model_.reset(new WrenchMenuModel(this, browser_));

  back_ = new views::ButtonDropDown(this, back_menu_model_.get());
  back_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                     views::Event::EF_MIDDLE_BUTTON_DOWN);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetID(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ButtonDropDown(this, forward_menu_model_.get());
  forward_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                        views::Event::EF_MIDDLE_BUTTON_DOWN);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetID(VIEW_ID_FORWARD_BUTTON);

  home_ = new views::ImageButton(this);
  home_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                     views::Event::EF_MIDDLE_BUTTON_DOWN);
  home_->set_tag(IDC_HOME);
  home_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_HOME));
  home_->SetID(VIEW_ID_HOME_BUTTON);

  // Have to create this before |reload_| as |reload_|'s constructor needs it.
  location_bar_ = new LocationBarView(profile, browser_->command_updater(),
      model_, this, (display_mode_ == DISPLAYMODE_LOCATION) ?
          LocationBarView::POPUP : LocationBarView::NORMAL);

  reload_ = new ReloadButton(location_bar_, browser_);
  reload_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                       views::Event::EF_MIDDLE_BUTTON_DOWN);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);

  browser_actions_ = new BrowserActionsContainer(browser_, this);

  app_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  app_menu_->EnableCanvasFlippingForRTLUI(true);
  app_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringF(IDS_APPMENU_TOOLTIP,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  app_menu_->SetID(VIEW_ID_APP_MENU);

  // Catch the case where the window is created after we detect a new version.
  if (Singleton<UpgradeDetector>::get()->notify_upgrade())
    ShowUpgradeReminder();

  LoadImages();

  // Always add children in order from left to right, for accessibility.
  AddChildView(back_);
  AddChildView(forward_);
  AddChildView(home_);
  AddChildView(reload_);
  AddChildView(location_bar_);
  AddChildView(browser_actions_);
  AddChildView(app_menu_);

  location_bar_->Init();
  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void ToolbarView::SetProfile(Profile* profile) {
  if (profile != profile_) {
    profile_ = profile;
    location_bar_->SetProfile(profile);
  }
}

void ToolbarView::Update(TabContents* tab, bool should_restore_state) {
  if (location_bar_)
    location_bar_->Update(should_restore_state ? tab : NULL);

  if (browser_actions_)
    browser_actions_->RefreshBrowserActionViews();
}

void ToolbarView::SetToolbarFocusAndFocusLocationBar(int view_storage_id) {
  SetToolbarFocus(view_storage_id, location_bar_);
}

void ToolbarView::SetToolbarFocusAndFocusAppMenu(int view_storage_id) {
  SetToolbarFocus(view_storage_id, app_menu_);
}

void ToolbarView::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.push_back(listener);
}

void ToolbarView::RemoveMenuListener(views::MenuListener* listener) {
  for (std::vector<views::MenuListener*>::iterator i(menu_listeners_.begin());
       i != menu_listeners_.end(); ++i) {
    if (*i == listener) {
      menu_listeners_.erase(i);
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, AccessibleToolbarView overrides:

bool ToolbarView::SetToolbarFocus(
    int view_storage_id, views::View* initial_focus) {
  if (!AccessibleToolbarView::SetToolbarFocus(view_storage_id, initial_focus))
    return false;

  location_bar_->SetShowFocusRect(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, Menu::BaseControllerDelegate overrides:

bool ToolbarView::GetAcceleratorInfo(int id, menus::Accelerator* accel) {
  return GetWidget()->GetAccelerator(id, accel);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::MenuDelegate implementation:

void ToolbarView::RunMenu(views::View* source, const gfx::Point& /*pt*/) {
  DCHECK_EQ(VIEW_ID_APP_MENU, source->GetID());

  bool destroyed_flag = false;
  destroyed_flag_ = &destroyed_flag;
  wrench_menu_.reset(new WrenchMenu(browser_));
  wrench_menu_->Init(app_menu_model_.get());

  for (size_t i = 0; i < menu_listeners_.size(); ++i)
    menu_listeners_[i]->OnMenuOpened();

  wrench_menu_->RunMenu(app_menu_);

  if (destroyed_flag)
    return;
  destroyed_flag_ = NULL;

  // Stop pulsating the upgrade reminder on the app menu, if active.
  upgrade_reminder_pulse_timer_.Stop();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

TabContents* ToolbarView::GetTabContents() {
  return browser_->GetSelectedTabContents();
}

void ToolbarView::OnInputInProgress(bool in_progress) {
  // The edit should make sure we're only notified when something changes.
  DCHECK(model_->input_in_progress() != in_progress);

  model_->set_input_in_progress(in_progress);
  location_bar_->Update(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, AnimationDelegate implementation:

void ToolbarView::AnimationProgressed(const Animation* animation) {
  app_menu_->SetIcon(GetAppMenuIcon());
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandUpdater::CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  views::Button* button = NULL;
  switch (id) {
    case IDC_BACK:
      button = back_;
      break;
    case IDC_FORWARD:
      button = forward_;
      break;
    case IDC_HOME:
      button = home_;
      break;
  }
  if (button)
    button->SetEnabled(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::Button::ButtonListener implementation:

void ToolbarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  int command = sender->tag();
  WindowOpenDisposition disposition =
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags());
  if ((disposition == CURRENT_TAB) &&
      ((command == IDC_BACK) || (command == IDC_FORWARD))) {
    // Forcibly reset the location bar, since otherwise it won't discard any
    // ongoing user edits, since it doesn't realize this is a user-initiated
    // action.
    location_bar_->Revert();
  }
  browser_->ExecuteCommandWithDisposition(command, disposition);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, NotificationObserver implementation:

void ToolbarView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      Layout();
      SchedulePaint();
    }
  } else if (type == NotificationType::UPGRADE_RECOMMENDED) {
    ShowUpgradeReminder();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, menus::SimpleMenuModel::Delegate implementation:

bool ToolbarView::IsCommandIdChecked(int command_id) const {
#if defined(OS_CHROMEOS)
  if (command_id == IDC_TOGGLE_VERTICAL_TABS) {
    return browser_->UseVerticalTabs();
  }
#endif
  return (command_id == IDC_SHOW_BOOKMARK_BAR) &&
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool ToolbarView::IsCommandIdEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool ToolbarView::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = views::Accelerator(base::VKEY_X, false, true, false);
      return true;
    case IDC_COPY:
      *accelerator = views::Accelerator(base::VKEY_C, false, true, false);
      return true;
    case IDC_PASTE:
      *accelerator = views::Accelerator(base::VKEY_V, false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void ToolbarView::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

gfx::Size ToolbarView::GetPreferredSize() {
  if (IsDisplayModeNormal()) {
    int min_width = kControlIndent + back_->GetPreferredSize().width() +
        forward_->GetPreferredSize().width() + kControlHorizOffset +
        (show_home_button_.GetValue() ?
            (home_->GetPreferredSize().width() + kControlHorizOffset) : 0) +
        reload_->GetPreferredSize().width() + kControlHorizOffset +
        browser_actions_->GetPreferredSize().width() +
        kMenuButtonOffset +
        app_menu_->GetPreferredSize().width() + kPaddingRight;

    static SkBitmap normal_background;
    if (normal_background.isNull()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      normal_background = *rb.GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
    }

    return gfx::Size(min_width, normal_background.height());
  }

  int vertical_spacing = PopupTopSpacing() +
      (GetWindow()->GetNonClientView()->UseNativeFrame() ?
          kPopupBottomSpacingGlass : kPopupBottomSpacingNonGlass);
  return gfx::Size(0, location_bar_->GetPreferredSize().height() +
      vertical_spacing);
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (back_ == NULL)
    return;

  if (!IsDisplayModeNormal()) {
    int edge_width = (browser_->window() && browser_->window()->IsMaximized()) ?
        0 : kPopupBackgroundEdge->width();  // See Paint().
    location_bar_->SetBounds(edge_width, PopupTopSpacing(),
        width() - (edge_width * 2), location_bar_->GetPreferredSize().height());
    return;
  }

  int child_y = std::min(kControlVertOffset, height());
  // We assume all child elements are the same height.
  int child_height =
      std::min(back_->GetPreferredSize().height(), height() - child_y);

  // If the window is maximized, we extend the back button to the left so that
  // clicking on the left-most pixel will activate the back button.
  // TODO(abarth):  If the window becomes maximized but is not resized,
  //                then Layout() might not be called and the back button
  //                will be slightly the wrong size.  We should force a
  //                Layout() in this case.
  //                http://crbug.com/5540
  int back_width = back_->GetPreferredSize().width();
  if (browser_->window() && browser_->window()->IsMaximized())
    back_->SetBounds(0, child_y, back_width + kControlIndent, child_height);
  else
    back_->SetBounds(kControlIndent, child_y, back_width, child_height);

  forward_->SetBounds(back_->x() + back_->width(), child_y,
                      forward_->GetPreferredSize().width(), child_height);

  if (show_home_button_.GetValue()) {
    home_->SetVisible(true);
    home_->SetBounds(forward_->x() + forward_->width() + kControlHorizOffset,
                     child_y, home_->GetPreferredSize().width(), child_height);
  } else {
    home_->SetVisible(false);
    home_->SetBounds(forward_->x() + forward_->width(), child_y, 0,
                     child_height);
  }

  reload_->SetBounds(home_->x() + home_->width() + kControlHorizOffset, child_y,
                     reload_->GetPreferredSize().width(), child_height);

  int browser_actions_width = browser_actions_->GetPreferredSize().width();
  int app_menu_width = app_menu_->GetPreferredSize().width();
  int location_x = reload_->x() + reload_->width() + kControlHorizOffset;
  int available_width = width() - kPaddingRight - app_menu_width -
      browser_actions_width - kMenuButtonOffset - location_x;

  location_bar_->SetBounds(location_x, child_y, std::max(available_width, 0),
                           child_height);
  int next_menu_x =
      location_bar_->x() + location_bar_->width() + kMenuButtonOffset;

  browser_actions_->SetBounds(next_menu_x, 0, browser_actions_width, height());
  // The browser actions need to do a layout explicitly, because when an
  // extension is loaded/unloaded/changed, BrowserActionContainer removes and
  // re-adds everything, regardless of whether it has a page action. For a
  // page action, browser action bounds do not change, as a result of which
  // SetBounds does not do a layout at all.
  // TODO(sidchat): Rework the above behavior so that explicit layout is not
  //                required.
  browser_actions_->Layout();
  next_menu_x += browser_actions_width;

  app_menu_->SetBounds(next_menu_x, child_y, app_menu_width, child_height);
}

void ToolbarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

  if (IsDisplayModeNormal())
    return;

  // In maximized mode, we don't draw the endcaps on the location bar, because
  // when they're flush against the edge of the screen they just look glitchy.
  if (!browser_->window() || !browser_->window()->IsMaximized()) {
    int top_spacing = PopupTopSpacing();
    canvas->DrawBitmapInt(*kPopupBackgroundEdge, 0, top_spacing);
    canvas->DrawBitmapInt(*kPopupBackgroundEdge,
                          width() - kPopupBackgroundEdge->width(), top_spacing);
  }

  // For glass, we need to draw a black line below the location bar to separate
  // it from the content area.  For non-glass, the NonClientView draws the
  // toolbar background below the location bar for us.
  if (GetWindow()->GetNonClientView()->UseNativeFrame())
    canvas->FillRectInt(SK_ColorBLACK, 0, height() - 1, width(), 1);
}

void ToolbarView::ThemeChanged() {
  LoadImages();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, protected:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar.
views::View* ToolbarView::GetDefaultFocusableChild() {
  return location_bar_;
}

void ToolbarView::RemoveToolbarFocus() {
  AccessibleToolbarView::RemoveToolbarFocus();
  location_bar_->SetShowFocusRect(false);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

int ToolbarView::PopupTopSpacing() const {
  return GetWindow()->GetNonClientView()->UseNativeFrame() ?
      0 : kPopupTopSpacingNonGlass;
}

void ToolbarView::LoadImages() {
  ThemeProvider* tp = GetThemeProvider();

  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(IDR_THEME_BUTTON_BACKGROUND);

  back_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_BACK));
  back_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_BACK_H));
  back_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_BACK_P));
  back_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_BACK_D));
  back_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BACK_MASK));

  forward_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_FORWARD));
  forward_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_FORWARD_H));
  forward_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_FORWARD_P));
  forward_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_FORWARD_D));
  forward_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_FORWARD_MASK));

  home_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_HOME));
  home_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_HOME_H));
  home_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_HOME_P));
  home_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BUTTON_MASK));

  reload_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STOP));
  reload_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STOP_H));
  reload_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STOP_P));
  reload_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BUTTON_MASK));

  app_menu_->SetIcon(GetAppMenuIcon());
}

void ToolbarView::ShowUpgradeReminder() {
  update_reminder_animation_.reset(new SlideAnimation(this));
  update_reminder_animation_->SetSlideDuration(kPulseDuration);

  // Then start the recurring timer for pulsating it.
  upgrade_reminder_pulse_timer_.Start(
      base::TimeDelta::FromMilliseconds(kPulsateEveryMs),
      this, &ToolbarView::PulsateUpgradeNotifier);
}

void ToolbarView::PulsateUpgradeNotifier() {
  // Start the pulsating animation.
  update_reminder_animation_->Reset(0.0);
  update_reminder_animation_->Show();
}

SkBitmap ToolbarView::GetAppMenuIcon() {
  ThemeProvider* tp = GetThemeProvider();

  SkBitmap icon = *tp->GetBitmapNamed(IDR_TOOLS);

  if (!Singleton<UpgradeDetector>::get()->notify_upgrade())
    return icon;

  // Draw the chrome app menu icon onto the canvas.
  scoped_ptr<gfx::CanvasSkia> canvas(
      new gfx::CanvasSkia(icon.width(), icon.height(), false));
  canvas->DrawBitmapInt(icon, 0, 0);

  SkBitmap badge;
  static bool has_faded_in = false;
  if (!has_faded_in) {
    SkBitmap* dot = tp->GetBitmapNamed(IDR_UPGRADE_DOT_INACTIVE);
    SkBitmap transparent;
    transparent.setConfig(dot->getConfig(), dot->width(), dot->height());
    transparent.allocPixels();
    transparent.eraseARGB(0, 0, 0, 0);
    badge = SkBitmapOperations::CreateBlendedBitmap(
        *dot, transparent, 1.0 - update_reminder_animation_->GetCurrentValue());
    if (update_reminder_animation_->GetCurrentValue() == 1.0)
      has_faded_in = true;
  } else {
    // Convert animation values that start from 0.0 and incrementally go
    // up to 1.0 into values that start in 0.0, go to 1.0 and then back
    // to 0.0 (to create a pulsing effect).
    double value = 1.0 -
                   abs(2.0 * update_reminder_animation_->GetCurrentValue() -
                       1.0);

    // Add the badge to it.
    badge = SkBitmapOperations::CreateBlendedBitmap(
        *tp->GetBitmapNamed(IDR_UPGRADE_DOT_INACTIVE),
        *tp->GetBitmapNamed(IDR_UPGRADE_DOT_ACTIVE),
        value);
  }

  canvas->DrawBitmapInt(badge, kUpgradeDotOffset,
                        icon.height() - badge.height());

  return canvas->ExtractBitmap();
}

void ToolbarView::ActivateMenuButton(views::MenuButton* menu_button) {
#if defined(OS_WIN)
  // On Windows, we have to explicitly clear the focus before opening
  // the pop-up menu, then set the focus again when it closes.
  GetFocusManager()->ClearFocus();
#elif defined(OS_LINUX)
  // Under GTK, opening a pop-up menu causes the main window to lose focus.
  // Focus is automatically returned when the menu closes.
  //
  // Make sure that the menu button being activated has focus, so that
  // when the user escapes from the menu without selecting anything, focus
  // will be returned here.
  if (!menu_button->HasFocus()) {
    menu_button->RequestFocus();
    GetFocusManager()->StoreFocusedView();
  }
#endif

  // Tell the menu button to activate, opening its pop-up menu.
  menu_button->Activate();

#if defined(OS_WIN)
  SetToolbarFocus(NULL, menu_button);
#endif
}
