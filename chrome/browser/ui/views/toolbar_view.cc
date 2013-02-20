// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar_view.h"

#include "base/debug/trace_event.h"
#include "base/i18n/number_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/extensions/disabled_extensions_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/home_button.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"
#include "chrome/browser/ui/views/wrench_menu.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/controls/button/button_dropdown.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/browser/ui/views/critical_notification_bubble_view.h"
#if !defined(USE_AURA)
#include "chrome/browser/ui/views/app_menu_button_win.h"
#endif
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/native_theme/native_theme_aura.h"
#endif

using content::UserMetricsAction;
using content::WebContents;

namespace {

// The edge graphics have some built-in spacing/shadowing, so we have to adjust
// our spacing to make it match.
const int kLeftEdgeSpacing = 3;
const int kRightEdgeSpacing = 2;

// The buttons to the left of the omnibox are close together.
const int kButtonSpacing = 0;

// Ash doesn't use a rounded content area and its top edge has an extra shadow.
const int kContentShadowHeightAsh = 2;

// Non-ash uses a rounded content area with no shadow in the assets.
const int kContentShadowHeight = 0;

const int kPopupTopSpacingNonGlass = 3;
const int kPopupBottomSpacingNonGlass = 2;
const int kPopupBottomSpacingGlass = 1;

// Top margin for the wrench menu badges (badge is placed in the upper right
// corner of the wrench menu).
const int kBadgeTopMargin = 2;

gfx::ImageSkia* kPopupBackgroundEdge = NULL;

// The omnibox border has some additional shadow, so we use less vertical
// spacing than ToolbarView::kVertSpacing.
int location_bar_vert_spacing() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = ToolbarView::kVertSpacing;
        break;
      case ui::LAYOUT_TOUCH:
        value = 10;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// The buttons to the left of the omnibox are close together.
int button_spacing() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = kButtonSpacing;
        break;
      case ui::LAYOUT_TOUCH:
        value = kButtonSpacing + 3;
        break;
    }
  }
  return value;
}

class BadgeImageSource: public gfx::CanvasImageSource {
 public:
  BadgeImageSource(const gfx::ImageSkia& icon, const gfx::ImageSkia& badge)
      : gfx::CanvasImageSource(icon.size(), false),
        icon_(icon),
        badge_(badge) {
  }

  virtual ~BadgeImageSource() {
  }

  // Overridden from gfx::CanvasImageSource:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawImageInt(icon_, 0, 0);
    canvas->DrawImageInt(badge_, icon_.width() - badge_.width(),
                         kBadgeTopMargin);
  }

 private:
  const gfx::ImageSkia icon_;
  const gfx::ImageSkia badge_;

  DISALLOW_COPY_AND_ASSIGN(BadgeImageSource);
};

}  // namespace

// static
const char ToolbarView::kViewClassName[] = "browser/ui/views/ToolbarView";
// The space between items is 3 px in general.
const int ToolbarView::kStandardSpacing = 3;
// The top of the toolbar has an edge we have to skip over in addition to the
// above spacing.
const int ToolbarView::kVertSpacing = 5;

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser)
    : model_(browser->toolbar_model()),
      back_(NULL),
      forward_(NULL),
      reload_(NULL),
      home_(NULL),
      location_bar_(NULL),
      browser_actions_(NULL),
      app_menu_(NULL),
      browser_(browser) {
  set_id(VIEW_ID_TOOLBAR);

  chrome::AddCommandObserver(browser_, IDC_BACK, this);
  chrome::AddCommandObserver(browser_, IDC_FORWARD, this);
  chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
  chrome::AddCommandObserver(browser_, IDC_HOME, this);
  chrome::AddCommandObserver(browser_, IDC_LOAD_NEW_TAB_PAGE, this);

  display_mode_ = browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ?
      DISPLAYMODE_NORMAL : DISPLAYMODE_LOCATION;

  if (!kPopupBackgroundEdge) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    kPopupBackgroundEdge = rb.GetImageSkiaNamed(IDR_LOCATIONBG_POPUPMODE_EDGE);
  }

  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllSources());
  if (OutdatedUpgradeBubbleView::IsAvailable()) {
    registrar_.Add(this, chrome::NOTIFICATION_OUTDATED_INSTALL,
                   content::NotificationService::AllSources());
  }
#if defined(OS_WIN)
  registrar_.Add(this, chrome::NOTIFICATION_CRITICAL_UPGRADE_INSTALLED,
                 content::NotificationService::AllSources());
#endif
  registrar_.Add(this,
                 chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_BADGE_CHANGE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(browser_->profile()));
}

ToolbarView::~ToolbarView() {
  // NOTE: Don't remove the command observers here.  This object gets destroyed
  // after the Browser (which owns the CommandUpdater), so the CommandUpdater is
  // already gone.
}

void ToolbarView::Init() {
  GetWidget()->AddObserver(this);

  back_ = new views::ButtonDropDown(this, new BackForwardMenuModel(
      browser_, BackForwardMenuModel::BACKWARD_MENU));
  back_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                     ui::EF_MIDDLE_MOUSE_BUTTON);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_->set_id(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ButtonDropDown(this, new BackForwardMenuModel(
      browser_, BackForwardMenuModel::FORWARD_MENU));
  forward_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                        ui::EF_MIDDLE_MOUSE_BUTTON);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward_->set_id(VIEW_ID_FORWARD_BUTTON);

  // Have to create this before |reload_| as |reload_|'s constructor needs it.
  location_bar_ = new LocationBarView(
      browser_,
      browser_->profile(),
      browser_->command_controller()->command_updater(),
      model_,
      this,
      (display_mode_ == DISPLAYMODE_LOCATION) ?
          LocationBarView::POPUP : LocationBarView::NORMAL);

  reload_ = new ReloadButton(location_bar_,
                             browser_->command_controller()->command_updater());
  reload_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload_->set_id(VIEW_ID_RELOAD_BUTTON);

  home_ = new HomeImageButton(this, browser_);
  home_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                     ui::EF_MIDDLE_MOUSE_BUTTON);
  home_->set_tag(IDC_HOME);
  home_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME));
  home_->set_id(VIEW_ID_HOME_BUTTON);

  browser_actions_ = new BrowserActionsContainer(browser_, this);

#if defined(OS_WIN) && !defined(USE_AURA)
  app_menu_ = new AppMenuButtonWin(this);
#else
  app_menu_ = new views::MenuButton(NULL, string16(), this, false);
#endif
  app_menu_->set_border(NULL);
  app_menu_->EnableCanvasFlippingForRTLUI(true);
  app_menu_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_APPMENU_TOOLTIP,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  app_menu_->set_id(VIEW_ID_APP_MENU);

  // Add any necessary badges to the menu item based on the system state.
  if (ShouldShowUpgradeRecommended() || ShouldShowIncompatibilityWarning())
    UpdateAppMenuState();
  LoadImages();

  // Always add children in order from left to right, for accessibility.
  AddChildView(back_);
  AddChildView(forward_);
  AddChildView(reload_);
  AddChildView(home_);
  AddChildView(location_bar_);
  AddChildView(browser_actions_);
  AddChildView(app_menu_);

  location_bar_->Init();
  show_home_button_.Init(prefs::kShowHomeButton,
                         browser_->profile()->GetPrefs(),
                         base::Bind(&ToolbarView::OnShowHomeButtonChanged,
                                    base::Unretained(this)));

  browser_actions_->Init();

  // Accessibility specific tooltip text.
  if (content::BrowserAccessibilityState::GetInstance()->
          IsAccessibleBrowser()) {
    back_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLTIP_BACK));
    forward_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLTIP_FORWARD));
  }
}

void ToolbarView::Update(WebContents* tab, bool should_restore_state) {
  if (location_bar_)
    location_bar_->Update(should_restore_state ? tab : NULL);

  if (browser_actions_)
    browser_actions_->RefreshBrowserActionViews();

  if (reload_)
    reload_->set_menu_enabled(chrome::IsDebuggerAttachedToCurrentTab(browser_));
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  SetPaneFocus(app_menu_);
}

bool ToolbarView::IsAppMenuFocused() {
  return app_menu_->HasFocus();
}

void ToolbarView::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.AddObserver(listener);
}

void ToolbarView::RemoveMenuListener(views::MenuListener* listener) {
  menu_listeners_.RemoveObserver(listener);
}

gfx::ImageSkia ToolbarView::GetAppMenuIcon(
    views::CustomButton::ButtonState state) {
  ui::ThemeProvider* tp = GetThemeProvider();

  int id = 0;
  switch (state) {
    case views::CustomButton::STATE_NORMAL:  id = IDR_TOOLS;   break;
    case views::CustomButton::STATE_HOVERED: id = IDR_TOOLS_H; break;
    case views::CustomButton::STATE_PRESSED: id = IDR_TOOLS_P; break;
    default:                                 NOTREACHED();     break;
  }
  gfx::ImageSkia icon = *tp->GetImageSkiaNamed(id);

#if defined(OS_WIN)
  // Keep track of whether we were showing the badge before, so we don't send
  // multiple UMA events for example when multiple Chrome windows are open.
  static bool incompatibility_badge_showing = false;
  // Save the old value before resetting it.
  bool was_showing = incompatibility_badge_showing;
  incompatibility_badge_showing = false;
#endif

  int error_badge_id = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetFirstBadgeResourceID();

  bool add_badge = ShouldShowUpgradeRecommended() ||
                   ShouldShowIncompatibilityWarning() || error_badge_id;
  if (!add_badge)
    return icon;

  gfx::ImageSkia badge;
  // Only one badge can be active at any given time. The Upgrade notification
  // is deemed most important, then the DLL conflict badge.
  if (ShouldShowUpgradeRecommended()) {
    badge = *tp->GetImageSkiaNamed(
        UpgradeDetector::GetInstance()->GetIconResourceID(
            UpgradeDetector::UPGRADE_ICON_TYPE_BADGE));
  } else if (ShouldShowIncompatibilityWarning()) {
#if defined(OS_WIN)
    if (!was_showing)
      content::RecordAction(UserMetricsAction("ConflictBadge"));
    badge = *tp->GetImageSkiaNamed(IDR_CONFLICT_BADGE);
    incompatibility_badge_showing = true;
#else
    NOTREACHED();
#endif
  } else if (error_badge_id) {
    badge = *tp->GetImageSkiaNamed(error_badge_id);
  } else {
    NOTREACHED();
  }

  gfx::CanvasImageSource* source = new BadgeImageSource(icon, badge);
  // ImageSkia takes ownership of |source|.
  return gfx::ImageSkia(source, source->size());
}

views::View* ToolbarView::GetBookmarkBubbleAnchor() {
  views::View* star_view = location_bar()->star_view();
  if (star_view && star_view->visible())
    return star_view;
  return app_menu_;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, AccessiblePaneView overrides:

bool ToolbarView::SetPaneFocus(views::View* initial_focus) {
  if (!AccessiblePaneView::SetPaneFocus(initial_focus))
    return false;

  location_bar_->SetShowFocusRect(true);
  return true;
}

void ToolbarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLBAR);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, Menu::Delegate overrides:

bool ToolbarView::GetAcceleratorInfo(int id, ui::Accelerator* accel) {
  return GetWidget()->GetAccelerator(id, accel);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::MenuButtonListener implementation:

void ToolbarView::OnMenuButtonClicked(views::View* source,
                                      const gfx::Point& point) {
  TRACE_EVENT0("ui::views", "ToolbarView::OnMenuButtonClicked");
  DCHECK_EQ(VIEW_ID_APP_MENU, source->id());

  bool use_new_menu = false;
  bool supports_new_separators = false;
  // TODO: remove this.
#if defined(USE_AURA)
  supports_new_separators =
      GetNativeTheme() == ui::NativeThemeAura::instance();
  use_new_menu = supports_new_separators;
#endif
#if defined(OS_WIN)
  use_new_menu = use_new_menu || ui::GetDisplayLayout() == ui::LAYOUT_TOUCH;
#endif

  wrench_menu_.reset(new WrenchMenu(browser_, use_new_menu,
                                    supports_new_separators));
  wrench_menu_model_.reset(new WrenchMenuModel(this, browser_, use_new_menu,
                                               supports_new_separators));
  wrench_menu_->Init(wrench_menu_model_.get());

  FOR_EACH_OBSERVER(views::MenuListener, menu_listeners_, OnMenuOpened());

  wrench_menu_->RunMenu(app_menu_);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

InstantController* ToolbarView::GetInstant() {
  return browser_->instant_controller() ?
      browser_->instant_controller()->instant() : NULL;
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_->content_setting_bubble_model_delegate();
}

void ToolbarView::ShowWebsiteSettings(content::WebContents* web_contents,
                                      const GURL& url,
                                      const content::SSLStatus& ssl,
                                      bool show_history) {
  chrome::ShowWebsiteSettings(browser_, web_contents, url, ssl, show_history);
}

views::Widget* ToolbarView::CreateViewsBubble(
    views::BubbleDelegateView* bubble_delegate) {
  return views::BubbleDelegateView::CreateBubble(bubble_delegate);
}

PageActionImageView* ToolbarView::CreatePageActionImageView(
    LocationBarView* owner, ExtensionAction* action) {
  return new PageActionImageView(owner, action, browser_);
}

void ToolbarView::OnInputInProgress(bool in_progress) {
  // The edit should make sure we're only notified when something changes.
  DCHECK(model_->GetInputInProgress() != in_progress);

  model_->SetInputInProgress(in_progress);
  location_bar_->Update(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  views::Button* button = NULL;
  switch (id) {
    case IDC_BACK:
      button = back_;
      break;
    case IDC_FORWARD:
      button = forward_;
      break;
    case IDC_RELOAD:
      button = reload_;
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
                                const ui::Event& event) {
  int command = sender->tag();
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event.flags());
  if ((disposition == CURRENT_TAB) &&
      ((command == IDC_BACK) || (command == IDC_FORWARD))) {
    // Forcibly reset the location bar, since otherwise it won't discard any
    // ongoing user edits, since it doesn't realize this is a user-initiated
    // action.
    location_bar_->Revert();
  }
  chrome::ExecuteCommandWithDisposition(browser_, command, disposition);
}

void ToolbarView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  if (visible) {
    DisabledExtensionsView::MaybeShow(browser_, app_menu_);
    GetWidget()->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, content::NotificationObserver implementation:

void ToolbarView::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED:
    case chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_BADGE_CHANGE:
    case chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED:
      UpdateAppMenuState();
      break;
    case chrome::NOTIFICATION_OUTDATED_INSTALL:
      ShowOutdatedInstallNotification();
      break;
#if defined(OS_WIN)
    case chrome::NOTIFICATION_CRITICAL_UPGRADE_INSTALLED:
      ShowCriticalNotification();
      break;
#endif
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, ui::AcceleratorProvider implementation:

bool ToolbarView::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;
#if defined(USE_ASH)
    // When USE_ASH is defined, the commands listed here are handled outside
    // Chrome, in ash/accelerators/accelerator_table.cc (crbug.com/120196).
    case IDC_CLEAR_BROWSING_DATA:
      *accelerator = ui::Accelerator(ui::VKEY_BACK,
                                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_NEW_TAB:
      *accelerator = ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_NEW_WINDOW:
      *accelerator = ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_NEW_INCOGNITO_WINDOW:
      *accelerator = ui::Accelerator(ui::VKEY_N,
                                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_TASK_MANAGER:
      *accelerator = ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN);
      return true;
    case IDC_FEEDBACK:
      *accelerator = ui::Accelerator(ui::VKEY_I,
                                     ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
      return true;
#endif
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

gfx::Size ToolbarView::GetPreferredSize() {
  if (is_display_mode_normal()) {
    int min_width = kLeftEdgeSpacing +
        back_->GetPreferredSize().width() + button_spacing() +
        forward_->GetPreferredSize().width() + button_spacing() +
        reload_->GetPreferredSize().width() + kStandardSpacing +
        (show_home_button_.GetValue() ?
            (home_->GetPreferredSize().width() + button_spacing()) : 0) +
        location_bar_->GetPreferredSize().width() +
        browser_actions_->GetPreferredSize().width() +
        app_menu_->GetPreferredSize().width() + kRightEdgeSpacing;
    gfx::ImageSkia* normal_background =
        GetThemeProvider()->GetImageSkiaNamed(IDR_CONTENT_TOP_CENTER);
    return gfx::Size(min_width,
                     normal_background->height() - content_shadow_height());
  }

  int vertical_spacing = PopupTopSpacing() +
      (GetWidget()->ShouldUseNativeFrame() ?
          kPopupBottomSpacingGlass : kPopupBottomSpacingNonGlass);
  return gfx::Size(0, location_bar_->GetPreferredSize().height() +
      vertical_spacing);
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (back_ == NULL)
    return;

  bool maximized = browser_->window() && browser_->window()->IsMaximized();
  if (!is_display_mode_normal()) {
    int edge_width = maximized ?
        0 : kPopupBackgroundEdge->width();  // See OnPaint().
    location_bar_->SetBounds(edge_width, PopupTopSpacing(),
        width() - (edge_width * 2), location_bar_->GetPreferredSize().height());
    return;
  }

  // We assume all child elements are the same height.
  int child_height =
      std::min(back_->GetPreferredSize().height(), height());

  // Set child_y such that buttons appear vertically centered. To preseve
  // the behaviour on non-touch UIs, round-up by taking
  // ceil((height() - child_height) / 2) + delta
  // which is equivalent to the below.
  int child_y = (1 + ((height() - child_height - 1) / 2));

  // If the window is maximized, we extend the back button to the left so that
  // clicking on the left-most pixel will activate the back button.
  // TODO(abarth):  If the window becomes maximized but is not resized,
  //                then Layout() might not be called and the back button
  //                will be slightly the wrong size.  We should force a
  //                Layout() in this case.
  //                http://crbug.com/5540
  int back_width = back_->GetPreferredSize().width();
  if (maximized)
    back_->SetBounds(0, child_y, back_width + kLeftEdgeSpacing, child_height);
  else
    back_->SetBounds(kLeftEdgeSpacing, child_y, back_width, child_height);

  forward_->SetBounds(back_->x() + back_->width() + button_spacing(),
      child_y, forward_->GetPreferredSize().width(), child_height);

  reload_->SetBounds(forward_->x() + forward_->width() + button_spacing(),
      child_y, reload_->GetPreferredSize().width(), child_height);

  if (show_home_button_.GetValue()) {
    home_->SetVisible(true);
    home_->SetBounds(reload_->x() + reload_->width() + button_spacing(),
                     child_y, home_->GetPreferredSize().width(), child_height);
  } else {
    home_->SetVisible(false);
    home_->SetBounds(reload_->x() + reload_->width(), child_y, 0, child_height);
  }

  int browser_actions_width = browser_actions_->GetPreferredSize().width();
  int app_menu_width = app_menu_->GetPreferredSize().width();
  int location_x = home_->x() + home_->width() + kStandardSpacing;
  int available_width = std::max(0, width() - kRightEdgeSpacing -
      app_menu_width - browser_actions_width - location_x);

  int location_y = std::min(location_bar_vert_spacing(), height());
  int location_bar_height = location_bar_->GetPreferredSize().height();
  location_bar_->SetBounds(location_x, location_y, std::max(available_width, 0),
                           location_bar_height);

  browser_actions_->SetBounds(location_bar_->x() + location_bar_->width(), 0,
                              browser_actions_width, height());
  // The browser actions need to do a layout explicitly, because when an
  // extension is loaded/unloaded/changed, BrowserActionContainer removes and
  // re-adds everything, regardless of whether it has a page action. For a
  // page action, browser action bounds do not change, as a result of which
  // SetBounds does not do a layout at all.
  // TODO(sidchat): Rework the above behavior so that explicit layout is not
  //                required.
  browser_actions_->Layout();

  // Extend the app menu to the screen's right edge in maximized mode just like
  // we extend the back button to the left edge.
  if (maximized)
    app_menu_width += kRightEdgeSpacing;
  app_menu_->SetBounds(browser_actions_->x() + browser_actions_width, child_y,
                       app_menu_width, child_height);
}

bool ToolbarView::HitTestRect(const gfx::Rect& rect) const {
  // Don't take hits in our top shadow edge.  Let them fall through to the
  // tab strip above us.
  if (rect.y() < content_shadow_height())
    return false;
  // Otherwise let our superclass take care of it.
  return AccessiblePaneView::HitTestRect(rect);
}

void ToolbarView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (is_display_mode_normal())
    return;

  // In maximized mode, we don't draw the endcaps on the location bar, because
  // when they're flush against the edge of the screen they just look glitchy.
  if (!browser_->window() || !browser_->window()->IsMaximized()) {
    int top_spacing = PopupTopSpacing();
    canvas->DrawImageInt(*kPopupBackgroundEdge, 0, top_spacing);
    canvas->DrawImageInt(*kPopupBackgroundEdge,
                         width() - kPopupBackgroundEdge->width(), top_spacing);
  }

  // For glass, we need to draw a black line below the location bar to separate
  // it from the content area.  For non-glass, the NonClientView draws the
  // toolbar background below the location bar for us.
  // NOTE: Keep this in sync with BrowserView::GetInfoBarSeparatorColor()!
  if (GetWidget()->ShouldUseNativeFrame())
    canvas->FillRect(gfx::Rect(0, height() - 1, width(), 1), SK_ColorBLACK);
}

// Note this method is ignored on Windows, but needs to be implemented for
// linux, where it is called before CanDrop().
bool ToolbarView::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  *formats = ui::OSExchangeData::URL | ui::OSExchangeData::STRING;
  return true;
}

bool ToolbarView::CanDrop(const ui::OSExchangeData& data) {
  // To support loading URLs by dropping into the toolbar, we need to support
  // dropping URLs and/or text.
  return data.HasURL() || data.HasString();
}

int ToolbarView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (event.source_operations() & ui::DragDropTypes::DRAG_COPY) {
    return ui::DragDropTypes::DRAG_COPY;
  } else if (event.source_operations() & ui::DragDropTypes::DRAG_LINK) {
    return ui::DragDropTypes::DRAG_LINK;
  }
  return ui::DragDropTypes::DRAG_NONE;
}

int ToolbarView::OnPerformDrop(const ui::DropTargetEvent& event) {
  return location_bar_->GetLocationEntry()->OnPerformDrop(event);
}

void ToolbarView::OnThemeChanged() {
  LoadImages();
}

std::string ToolbarView::GetClassName() const {
  return kViewClassName;
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view == location_bar_)
    return false;  // Let location bar handle all accelerator events.
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

bool ToolbarView::IsWrenchMenuShowing() const {
  return wrench_menu_.get() && wrench_menu_->IsShowing();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, protected:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar - and
// also so that it selects all content in the location bar.
bool ToolbarView::SetPaneFocusAndFocusDefault() {
  if (!location_bar_->HasFocus()) {
    SetPaneFocus(location_bar_);
    location_bar_->FocusLocation(true);
    return true;
  }

  if (!AccessiblePaneView::SetPaneFocusAndFocusDefault())
    return false;
  browser_->window()->RotatePaneFocus(true);
  return true;
}

void ToolbarView::RemovePaneFocus() {
  AccessiblePaneView::RemovePaneFocus();
  location_bar_->SetShowFocusRect(false);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

bool ToolbarView::ShouldShowUpgradeRecommended() {
#if defined(OS_CHROMEOS)
  // In chromeos, the update recommendation is shown in the system tray. So it
  // should not be displayed in the wrench menu.
  return false;
#else
  return (UpgradeDetector::GetInstance()->notify_upgrade());
#endif
}

bool ToolbarView::ShouldShowIncompatibilityWarning() {
#if defined(OS_WIN)
  EnumerateModulesModel* loaded_modules = EnumerateModulesModel::GetInstance();
  return loaded_modules->ShouldShowConflictWarning();
#else
  return false;
#endif
}

int ToolbarView::PopupTopSpacing() const {
  return GetWidget()->ShouldUseNativeFrame() ? 0 : kPopupTopSpacingNonGlass;
}

void ToolbarView::LoadImages() {
  ui::ThemeProvider* tp = GetThemeProvider();

  back_->SetImage(views::CustomButton::STATE_NORMAL,
      tp->GetImageSkiaNamed(IDR_BACK));
  back_->SetImage(views::CustomButton::STATE_HOVERED,
      tp->GetImageSkiaNamed(IDR_BACK_H));
  back_->SetImage(views::CustomButton::STATE_PRESSED,
      tp->GetImageSkiaNamed(IDR_BACK_P));
  back_->SetImage(views::CustomButton::STATE_DISABLED,
      tp->GetImageSkiaNamed(IDR_BACK_D));

  forward_->SetImage(views::CustomButton::STATE_NORMAL,
      tp->GetImageSkiaNamed(IDR_FORWARD));
  forward_->SetImage(views::CustomButton::STATE_HOVERED,
      tp->GetImageSkiaNamed(IDR_FORWARD_H));
  forward_->SetImage(views::CustomButton::STATE_PRESSED,
      tp->GetImageSkiaNamed(IDR_FORWARD_P));
  forward_->SetImage(views::CustomButton::STATE_DISABLED,
      tp->GetImageSkiaNamed(IDR_FORWARD_D));

  reload_->LoadImages(tp);

  home_->SetImage(views::CustomButton::STATE_NORMAL,
      tp->GetImageSkiaNamed(IDR_HOME));
  home_->SetImage(views::CustomButton::STATE_HOVERED,
      tp->GetImageSkiaNamed(IDR_HOME_H));
  home_->SetImage(views::CustomButton::STATE_PRESSED,
      tp->GetImageSkiaNamed(IDR_HOME_P));

  app_menu_->SetIcon(GetAppMenuIcon(views::CustomButton::STATE_NORMAL));
  app_menu_->SetHoverIcon(GetAppMenuIcon(views::CustomButton::STATE_HOVERED));
  app_menu_->SetPushedIcon(GetAppMenuIcon(views::CustomButton::STATE_PRESSED));
}

void ToolbarView::ShowCriticalNotification() {
#if defined(OS_WIN)
  CriticalNotificationBubbleView* bubble_delegate =
      new CriticalNotificationBubbleView(app_menu_);
  views::BubbleDelegateView::CreateBubble(bubble_delegate);
  bubble_delegate->StartFade(true);
#endif
}

void ToolbarView::ShowOutdatedInstallNotification() {
  if (OutdatedUpgradeBubbleView::IsAvailable())
    OutdatedUpgradeBubbleView::ShowBubble(app_menu_, browser_);
}

void ToolbarView::UpdateAppMenuState() {
  string16 accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (ShouldShowUpgradeRecommended()) {
    accname_app = l10n_util::GetStringFUTF16(
        IDS_ACCNAME_APP_UPGRADE_RECOMMENDED, accname_app);
  }
  app_menu_->SetAccessibleName(accname_app);

  app_menu_->SetIcon(GetAppMenuIcon(views::CustomButton::STATE_NORMAL));
  app_menu_->SetHoverIcon(GetAppMenuIcon(views::CustomButton::STATE_HOVERED));
  app_menu_->SetPushedIcon(GetAppMenuIcon(views::CustomButton::STATE_PRESSED));
  SchedulePaint();
}

void ToolbarView::OnShowHomeButtonChanged() {
  Layout();
  SchedulePaint();
}

int ToolbarView::content_shadow_height() const {
  return browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH ?
      kContentShadowHeightAsh : kContentShadowHeight;
}
