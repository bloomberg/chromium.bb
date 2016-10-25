// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray_accessibility.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

enum AccessibilityState {
  A11Y_NONE = 0,
  A11Y_SPOKEN_FEEDBACK = 1 << 0,
  A11Y_HIGH_CONTRAST = 1 << 1,
  A11Y_SCREEN_MAGNIFIER = 1 << 2,
  A11Y_LARGE_CURSOR = 1 << 3,
  A11Y_AUTOCLICK = 1 << 4,
  A11Y_VIRTUAL_KEYBOARD = 1 << 5,
  A11Y_BRAILLE_DISPLAY_CONNECTED = 1 << 6,
};

uint32_t GetAccessibilityState() {
  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();
  uint32_t state = A11Y_NONE;
  if (delegate->IsSpokenFeedbackEnabled())
    state |= A11Y_SPOKEN_FEEDBACK;
  if (delegate->IsHighContrastEnabled())
    state |= A11Y_HIGH_CONTRAST;
  if (delegate->IsMagnifierEnabled())
    state |= A11Y_SCREEN_MAGNIFIER;
  if (delegate->IsLargeCursorEnabled())
    state |= A11Y_LARGE_CURSOR;
  if (delegate->IsAutoclickEnabled())
    state |= A11Y_AUTOCLICK;
  if (delegate->IsVirtualKeyboardEnabled())
    state |= A11Y_VIRTUAL_KEYBOARD;
  if (delegate->IsBrailleDisplayConnected())
    state |= A11Y_BRAILLE_DISPLAY_CONNECTED;
  return state;
}

LoginStatus GetCurrentLoginStatus() {
  return WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
}

}  // namespace

namespace tray {

class DefaultAccessibilityView : public TrayItemMore {
 public:
  explicit DefaultAccessibilityView(SystemTrayItem* owner)
      : TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    if (!MaterialDesignController::UseMaterialDesignSystemIcons()) {
      // The icon doesn't change in non-md.
      SetImage(*bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK)
                    .ToImageSkia());
    }
    base::string16 label =
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_ACCESSIBILITY);
    SetLabel(label);
    SetAccessibleName(label);
    set_id(test::kAccessibilityTrayItemViewId);
  }

  ~DefaultAccessibilityView() override {}

 protected:
  // TrayItemMore:
  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();

    if (!MaterialDesignController::IsSystemTrayMenuMaterial())
      return;

    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(gfx::CreateVectorIcon(kSystemMenuAccessibilityIcon,
                                   style->GetForegroundColor()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityView);
};

////////////////////////////////////////////////////////////////////////////////
// ash::tray::AccessibilityPopupView

AccessibilityPopupView::AccessibilityPopupView(SystemTrayItem* owner,
                                               uint32_t enabled_state_bits)
    : TrayNotificationView(owner, IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK),
      label_(CreateLabel(enabled_state_bits)) {
  InitView(label_);
}

views::Label* AccessibilityPopupView::CreateLabel(uint32_t enabled_state_bits) {
  DCHECK((enabled_state_bits &
          (A11Y_SPOKEN_FEEDBACK | A11Y_BRAILLE_DISPLAY_CONNECTED)) != 0);
  base::string16 text;
  if (enabled_state_bits & A11Y_BRAILLE_DISPLAY_CONNECTED) {
    text.append(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BRAILLE_DISPLAY_CONNECTED_BUBBLE));
  }
  if (enabled_state_bits & A11Y_SPOKEN_FEEDBACK) {
    if (!text.empty())
      text.append(base::ASCIIToUTF16(" "));
    text.append(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_BUBBLE));
  }
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

////////////////////////////////////////////////////////////////////////////////
// ash::tray::AccessibilityDetailedView

AccessibilityDetailedView::AccessibilityDetailedView(SystemTrayItem* owner,
                                                     LoginStatus login)
    : TrayDetailsView(owner),
      spoken_feedback_view_(nullptr),
      high_contrast_view_(nullptr),
      screen_magnifier_view_(nullptr),
      large_cursor_view_(nullptr),
      help_view_(nullptr),
      settings_view_(nullptr),
      autoclick_view_(nullptr),
      virtual_keyboard_view_(nullptr),
      spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      screen_magnifier_enabled_(false),
      large_cursor_enabled_(false),
      autoclick_enabled_(false),
      virtual_keyboard_enabled_(false),
      login_(login) {
  Reset();

  AppendAccessibilityList();

  if (!MaterialDesignController::IsSystemTrayMenuMaterial())
    AppendHelpEntries();

  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);

  Layout();
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();
  spoken_feedback_enabled_ = delegate->IsSpokenFeedbackEnabled();
  spoken_feedback_view_ =
      AddScrollListItem(bundle.GetLocalizedString(
                            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
                        spoken_feedback_enabled_, spoken_feedback_enabled_,
                        kSystemMenuAccessibilityChromevoxIcon);

  // Large Cursor item is shown only in Login screen.
  if (login_ == LoginStatus::NOT_LOGGED_IN) {
    large_cursor_enabled_ = delegate->IsLargeCursorEnabled();
    large_cursor_view_ =
        AddScrollListItem(bundle.GetLocalizedString(
                              IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR),
                          large_cursor_enabled_, large_cursor_enabled_,
                          kSystemMenuAccessibilityLargeCursorIcon);
  }

  high_contrast_enabled_ = delegate->IsHighContrastEnabled();
  high_contrast_view_ = AddScrollListItem(
      bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE),
      high_contrast_enabled_, high_contrast_enabled_,
      kSystemMenuAccessibilityContrastIcon);
  screen_magnifier_enabled_ = delegate->IsMagnifierEnabled();
  screen_magnifier_view_ =
      AddScrollListItem(bundle.GetLocalizedString(
                            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER),
                        screen_magnifier_enabled_, screen_magnifier_enabled_,
                        kSystemMenuAccessibilityScreenMagnifierIcon);

  // Don't show autoclick option at login screen.
  if (login_ != LoginStatus::NOT_LOGGED_IN) {
    autoclick_enabled_ = delegate->IsAutoclickEnabled();
    autoclick_view_ = AddScrollListItem(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK),
        autoclick_enabled_, autoclick_enabled_,
        kSystemMenuAccessibilityAutoClickIcon);
  }

  virtual_keyboard_enabled_ = delegate->IsVirtualKeyboardEnabled();
  virtual_keyboard_view_ =
      AddScrollListItem(bundle.GetLocalizedString(
                            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD),
                        virtual_keyboard_enabled_, virtual_keyboard_enabled_,
                        kSystemMenuKeyboardIcon);
}

void AccessibilityDetailedView::AppendHelpEntries() {
  // Currently the help page requires a browser window.
  // TODO(yoshiki): show this even on login/lock screen. crbug.com/158286
  if (!CanOpenWebUISettings(login_))
    return;

  views::View* bottom_row = new View();
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kTrayMenuBottomRowPadding,
      kTrayMenuBottomRowPadding, kTrayMenuBottomRowPaddingBetweenItems);
  layout->SetDefaultFlex(1);
  bottom_row->SetLayoutManager(layout);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  TrayPopupLabelButton* help = new TrayPopupLabelButton(
      this,
      bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LEARN_MORE));
  bottom_row->AddChildView(help);
  help_view_ = help;

  TrayPopupLabelButton* settings = new TrayPopupLabelButton(
      this,
      bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SETTINGS));
  bottom_row->AddChildView(settings);
  settings_view_ = settings;

  AddChildView(bottom_row);
}

HoverHighlightView* AccessibilityDetailedView::AddScrollListItem(
    const base::string16& text,
    bool highlight,
    bool checked,
    const gfx::VectorIcon& icon) {
  HoverHighlightView* container = new HoverHighlightView(this);
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    gfx::ImageSkia image = CreateVectorIcon(icon, kMenuIconColor);
    const int padding = (kMenuButtonSize - image.width()) / 2;
    container->AddIconAndLabelCustomSize(
        image, text, highlight,
        image.width() + kMenuSeparatorVerticalPadding * 2, padding, padding);
    gfx::ImageSkia check_mark =
        CreateVectorIcon(gfx::VectorIconId::CHECK_CIRCLE, gfx::kGoogleGreen700);
    container->AddRightIcon(check_mark, check_mark.width());
    container->SetRightIconVisible(checked);
  } else {
    container->AddCheckableLabel(text, highlight, checked);
  }
  scroll_content()->AddChildView(container);
  return container;
}

void AccessibilityDetailedView::HandleViewClicked(views::View* view) {
  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();
  UserMetricsAction user_action;
  if (view == spoken_feedback_view_) {
    user_action = delegate->IsSpokenFeedbackEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_SPOKEN_FEEDBACK
                      : ash::UMA_STATUS_AREA_ENABLE_SPOKEN_FEEDBACK;
    delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  } else if (view == high_contrast_view_) {
    user_action = delegate->IsHighContrastEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_HIGH_CONTRAST
                      : ash::UMA_STATUS_AREA_ENABLE_HIGH_CONTRAST;
    delegate->ToggleHighContrast();
  } else if (view == screen_magnifier_view_) {
    user_action = delegate->IsMagnifierEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_MAGNIFIER
                      : ash::UMA_STATUS_AREA_ENABLE_MAGNIFIER;
    delegate->SetMagnifierEnabled(!delegate->IsMagnifierEnabled());
  } else if (large_cursor_view_ && view == large_cursor_view_) {
    user_action = delegate->IsLargeCursorEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_LARGE_CURSOR
                      : ash::UMA_STATUS_AREA_ENABLE_LARGE_CURSOR;
    delegate->SetLargeCursorEnabled(!delegate->IsLargeCursorEnabled());
  } else if (autoclick_view_ && view == autoclick_view_) {
    user_action = delegate->IsAutoclickEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_AUTO_CLICK
                      : ash::UMA_STATUS_AREA_ENABLE_AUTO_CLICK;
    delegate->SetAutoclickEnabled(!delegate->IsAutoclickEnabled());
  } else if (virtual_keyboard_view_ && view == virtual_keyboard_view_) {
    user_action = delegate->IsVirtualKeyboardEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_VIRTUAL_KEYBOARD
                      : ash::UMA_STATUS_AREA_ENABLE_VIRTUAL_KEYBOARD;
    delegate->SetVirtualKeyboardEnabled(!delegate->IsVirtualKeyboardEnabled());
  } else {
    return;
  }
  WmShell::Get()->RecordUserMetricsAction(user_action);
}

void AccessibilityDetailedView::HandleButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  if (sender == help_view_)
    ShowHelp();
  else if (sender == settings_view_)
    ShowSettings();
}

void AccessibilityDetailedView::CreateExtraTitleRowButtons() {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    help_view_ = title_row()->AddHelpButton(this, login_);
    settings_view_ = title_row()->AddSettingsButton(this, login_);
  }
}

void AccessibilityDetailedView::ShowSettings() {
  if (CanOpenWebUISettings(login_)) {
    WmShell::Get()->system_tray_controller()->ShowAccessibilitySettings();
    owner()->system_tray()->CloseSystemBubble();
  }
}

void AccessibilityDetailedView::ShowHelp() {
  if (CanOpenWebUISettings(login_)) {
    WmShell::Get()->system_tray_controller()->ShowAccessibilityHelp();
    owner()->system_tray()->CloseSystemBubble();
  }
}

}  // namespace tray

////////////////////////////////////////////////////////////////////////////////
// ash::TrayAccessibility

TrayAccessibility::TrayAccessibility(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    IDR_AURA_UBER_TRAY_ACCESSIBILITY,
                    UMA_ACCESSIBILITY),
      default_(NULL),
      detailed_popup_(NULL),
      detailed_menu_(NULL),
      request_popup_view_state_(A11Y_NONE),
      tray_icon_visible_(false),
      login_(GetCurrentLoginStatus()),
      previous_accessibility_state_(GetAccessibilityState()),
      show_a11y_menu_on_lock_screen_(true) {
  DCHECK(system_tray);
  WmShell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
}

TrayAccessibility::~TrayAccessibility() {
  WmShell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(this);
}

void TrayAccessibility::SetTrayIconVisible(bool visible) {
  if (tray_view())
    tray_view()->SetVisible(visible);
  tray_icon_visible_ = visible;
}

tray::AccessibilityDetailedView* TrayAccessibility::CreateDetailedMenu() {
  return new tray::AccessibilityDetailedView(this, login_);
}

bool TrayAccessibility::GetInitialVisibility() {
  // Shows accessibility icon if any accessibility feature is enabled.
  // Otherwise, doen't show it.
  return GetAccessibilityState() != A11Y_NONE;
}

views::View* TrayAccessibility::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == NULL);

  // Shows accessibility menu if:
  // - on login screen (not logged in);
  // - "Enable accessibility menu" on chrome://settings is checked;
  // - or any of accessibility features is enabled
  // Otherwise, not shows it.
  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();
  if (login_ != LoginStatus::NOT_LOGGED_IN &&
      !delegate->ShouldShowAccessibilityMenu() &&
      // On login screen, keeps the initial visibility of the menu.
      (status != LoginStatus::LOCKED || !show_a11y_menu_on_lock_screen_))
    return NULL;

  CHECK(default_ == NULL);
  default_ = new tray::DefaultAccessibilityView(this);

  return default_;
}

views::View* TrayAccessibility::CreateDetailedView(LoginStatus status) {
  CHECK(detailed_popup_ == NULL);
  CHECK(detailed_menu_ == NULL);

  if (request_popup_view_state_) {
    detailed_popup_ =
        new tray::AccessibilityPopupView(this, request_popup_view_state_);
    request_popup_view_state_ = A11Y_NONE;
    return detailed_popup_;
  } else {
    WmShell::Get()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_DETAILED_ACCESSABILITY);
    detailed_menu_ = CreateDetailedMenu();
    return detailed_menu_;
  }
}

void TrayAccessibility::DestroyDefaultView() {
  default_ = NULL;
}

void TrayAccessibility::DestroyDetailedView() {
  detailed_popup_ = NULL;
  detailed_menu_ = NULL;
}

void TrayAccessibility::UpdateAfterLoginStatusChange(LoginStatus status) {
  // Stores the a11y feature status on just entering the lock screen.
  if (login_ != LoginStatus::LOCKED && status == LoginStatus::LOCKED)
    show_a11y_menu_on_lock_screen_ = (GetAccessibilityState() != A11Y_NONE);

  login_ = status;
  SetTrayIconVisible(GetInitialVisibility());
}

void TrayAccessibility::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  SetTrayIconVisible(GetInitialVisibility());

  uint32_t accessibility_state = GetAccessibilityState();
  // We'll get an extra notification if a braille display is connected when
  // spoken feedback wasn't already enabled.  This is because the braille
  // connection state is already updated when spoken feedback is enabled so
  // that the notifications can be consolidated into one.  Therefore, we
  // return early if there's no change in the state that we keep track of.
  if (accessibility_state == previous_accessibility_state_)
    return;
  // Contains bits for spoken feedback and braille display connected currently
  // being enabled.
  uint32_t being_enabled =
      (accessibility_state & ~previous_accessibility_state_) &
      (A11Y_SPOKEN_FEEDBACK | A11Y_BRAILLE_DISPLAY_CONNECTED);
  if ((notify == A11Y_NOTIFICATION_SHOW) && being_enabled != A11Y_NONE) {
    // Shows popup if |notify| is true and the spoken feedback is being enabled.
    request_popup_view_state_ = being_enabled;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else {
    if (detailed_popup_)
      detailed_popup_->GetWidget()->Close();
    if (detailed_menu_)
      detailed_menu_->GetWidget()->Close();
  }

  previous_accessibility_state_ = accessibility_state;
}

}  // namespace ash
