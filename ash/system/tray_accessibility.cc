// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/accessibility_delegate.h"
#include "ash/accessibility_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
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
  A11Y_MONO_AUDIO = 1 << 7,
  A11Y_CARET_HIGHLIGHT = 1 << 8,
  A11Y_HIGHLIGHT_MOUSE_CURSOR = 1 << 9,
  A11Y_HIGHLIGHT_KEYBOARD_FOCUS = 1 << 10,
  A11Y_STICKY_KEYS = 1 << 11,
  A11Y_TAP_DRAGGING = 1 << 12,
};

uint32_t GetAccessibilityState() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
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
  if (delegate->IsMonoAudioEnabled())
    state |= A11Y_MONO_AUDIO;
  if (delegate->IsCaretHighlightEnabled())
    state |= A11Y_CARET_HIGHLIGHT;
  if (delegate->IsCursorHighlightEnabled())
    state |= A11Y_HIGHLIGHT_MOUSE_CURSOR;
  if (delegate->IsFocusHighlightEnabled())
    state |= A11Y_HIGHLIGHT_KEYBOARD_FOCUS;
  if (delegate->IsStickyKeysEnabled())
    state |= A11Y_STICKY_KEYS;
  if (delegate->IsTapDraggingEnabled())
    state |= A11Y_TAP_DRAGGING;
  return state;
}

LoginStatus GetCurrentLoginStatus() {
  return Shell::Get()->session_controller()->login_status();
}

}  // namespace

namespace tray {

class DefaultAccessibilityView : public TrayItemMore {
 public:
  explicit DefaultAccessibilityView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    base::string16 label =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY);
    SetLabel(label);
    SetAccessibleName(label);
    set_id(test::kAccessibilityTrayItemViewId);
  }

  ~DefaultAccessibilityView() override {}

 protected:
  // TrayItemMore:
  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();
    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(gfx::CreateVectorIcon(kSystemMenuAccessibilityIcon,
                                   style->GetIconColor()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityView);
};

////////////////////////////////////////////////////////////////////////////////
// ash::tray::AccessibilityPopupView

AccessibilityPopupView::AccessibilityPopupView(uint32_t enabled_state_bits)
    : label_(CreateLabel(enabled_state_bits)) {}

void AccessibilityPopupView::Init() {
  set_background(views::Background::CreateThemedSolidBackground(
      this, ui::NativeTheme::kColorId_BubbleBackground));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ImageView* close_button = new views::ImageView();
  close_button->SetImage(
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_MESSAGE_CLOSE));
  close_button->SetHorizontalAlignment(views::ImageView::CENTER);
  close_button->SetVerticalAlignment(views::ImageView::CENTER);

  views::ImageView* icon = new views::ImageView;
  icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuAccessibilityIcon, kMenuIconColor));

  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Icon
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::FIXED, kNotificationIconWidth,
                     kNotificationIconWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Contents
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, /* resize percent */
                     views::GridLayout::FIXED, kTrayNotificationContentsWidth,
                     kTrayNotificationContentsWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Close button
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED, kNotificationButtonWidth,
                     kNotificationButtonWidth);

  // Layout rows
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  layout->StartRow(0, 0);
  layout->AddView(icon);
  layout->AddView(label_);
  layout->AddView(close_button);
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
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

AccessibilityDetailedView::AccessibilityDetailedView(SystemTrayItem* owner)
    : TrayDetailsView(owner) {
  Reset();
  AppendAccessibilityList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);
  Layout();
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();

  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();

  spoken_feedback_enabled_ = delegate->IsSpokenFeedbackEnabled();
  spoken_feedback_view_ = AddScrollListCheckableItem(
      kSystemMenuAccessibilityChromevoxIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
      spoken_feedback_enabled_);

  high_contrast_enabled_ = delegate->IsHighContrastEnabled();
  high_contrast_view_ = AddScrollListCheckableItem(
      kSystemMenuAccessibilityContrastIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE),
      high_contrast_enabled_);

  screen_magnifier_enabled_ = delegate->IsMagnifierEnabled();
  screen_magnifier_view_ = AddScrollListCheckableItem(
      kSystemMenuAccessibilityScreenMagnifierIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER),
      screen_magnifier_enabled_);

  autoclick_enabled_ = delegate->IsAutoclickEnabled();
  autoclick_view_ = AddScrollListCheckableItem(
      kSystemMenuAccessibilityAutoClickIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK),
      autoclick_enabled_);

  virtual_keyboard_enabled_ = delegate->IsVirtualKeyboardEnabled();
  virtual_keyboard_view_ = AddScrollListCheckableItem(
      kSystemMenuKeyboardIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD),
      virtual_keyboard_enabled_);

  scroll_content()->AddChildView(
      TrayPopupUtils::CreateListSubHeaderSeparator());

  AddScrollListSubHeader(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ADDITIONAL_SETTINGS);

  large_cursor_enabled_ = delegate->IsLargeCursorEnabled();
  large_cursor_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR),
      large_cursor_enabled_);

  mono_audio_enabled_ = delegate->IsMonoAudioEnabled();
  mono_audio_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MONO_AUDIO),
      mono_audio_enabled_);

  caret_highlight_enabled_ = delegate->IsCaretHighlightEnabled();
  caret_highlight_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT),
      caret_highlight_enabled_);

  highlight_mouse_cursor_enabled_ = delegate->IsCursorHighlightEnabled();
  highlight_mouse_cursor_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR),
      highlight_mouse_cursor_enabled_);

  // Focus highlighting can't be on when spoken feedback is on because
  // ChromeVox does its own focus highlighting.
  if (!spoken_feedback_enabled_) {
    highlight_keyboard_focus_enabled_ = delegate->IsFocusHighlightEnabled();
    highlight_keyboard_focus_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS),
        highlight_keyboard_focus_enabled_);
  }

  sticky_keys_enabled_ = delegate->IsStickyKeysEnabled();
  sticky_keys_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_STICKY_KEYS),
      sticky_keys_enabled_);

  tap_dragging_enabled_ = delegate->IsTapDraggingEnabled();
  tap_dragging_view_ = AddScrollListCheckableItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TAP_DRAGGING),
      tap_dragging_enabled_);
}

void AccessibilityDetailedView::HandleViewClicked(views::View* view) {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
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
  } else if (caret_highlight_view_ && view == caret_highlight_view_) {
    user_action = delegate->IsCaretHighlightEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_CARET_HIGHLIGHT
                      : ash::UMA_STATUS_AREA_ENABLE_CARET_HIGHLIGHT;
    delegate->SetCaretHighlightEnabled(!delegate->IsCaretHighlightEnabled());
  } else if (mono_audio_view_ && view == mono_audio_view_) {
    user_action = delegate->IsMonoAudioEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_MONO_AUDIO
                      : ash::UMA_STATUS_AREA_ENABLE_MONO_AUDIO;
    delegate->SetMonoAudioEnabled(!delegate->IsMonoAudioEnabled());
  } else if (highlight_mouse_cursor_view_ &&
             view == highlight_mouse_cursor_view_) {
    user_action = delegate->IsCursorHighlightEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_HIGHLIGHT_MOUSE_CURSOR
                      : ash::UMA_STATUS_AREA_ENABLE_HIGHLIGHT_MOUSE_CURSOR;
    delegate->SetCursorHighlightEnabled(!delegate->IsCursorHighlightEnabled());
  } else if (highlight_keyboard_focus_view_ &&
             view == highlight_keyboard_focus_view_) {
    user_action = delegate->IsFocusHighlightEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_HIGHLIGHT_KEYBOARD_FOCUS
                      : ash::UMA_STATUS_AREA_ENABLE_HIGHLIGHT_KEYBOARD_FOCUS;
    delegate->SetFocusHighlightEnabled(!delegate->IsFocusHighlightEnabled());
  } else if (sticky_keys_view_ && view == sticky_keys_view_) {
    user_action = delegate->IsStickyKeysEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_STICKY_KEYS
                      : ash::UMA_STATUS_AREA_ENABLE_STICKY_KEYS;
    delegate->SetStickyKeysEnabled(!delegate->IsStickyKeysEnabled());
  } else if (tap_dragging_view_ && view == tap_dragging_view_) {
    user_action = delegate->IsTapDraggingEnabled()
                      ? ash::UMA_STATUS_AREA_DISABLE_TAP_DRAGGING
                      : ash::UMA_STATUS_AREA_ENABLE_TAP_DRAGGING;
    delegate->SetTapDraggingEnabled(!delegate->IsTapDraggingEnabled());
  } else {
    return;
  }
  ShellPort::Get()->RecordUserMetricsAction(user_action);
}

void AccessibilityDetailedView::HandleButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  if (sender == help_view_)
    ShowHelp();
  else if (sender == settings_view_)
    ShowSettings();
}

void AccessibilityDetailedView::CreateExtraTitleRowButtons() {
  DCHECK(!help_view_);
  DCHECK(!settings_view_);

  tri_view()->SetContainerVisible(TriView::Container::END, true);

  help_view_ = CreateHelpButton();
  settings_view_ =
      CreateSettingsButton(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SETTINGS);
  tri_view()->AddView(TriView::Container::END, help_view_);
  tri_view()->AddView(TriView::Container::END, settings_view_);
}

void AccessibilityDetailedView::ShowSettings() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_controller()->ShowAccessibilitySettings();
    owner()->system_tray()->CloseSystemBubble();
  }
}

void AccessibilityDetailedView::ShowHelp() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_controller()->ShowAccessibilityHelp();
    owner()->system_tray()->CloseSystemBubble();
  }
}

}  // namespace tray

////////////////////////////////////////////////////////////////////////////////
// ash::TrayAccessibility

TrayAccessibility::TrayAccessibility(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayAccessibilityIcon,
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
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
}

TrayAccessibility::~TrayAccessibility() {
  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(this);
}

void TrayAccessibility::SetTrayIconVisible(bool visible) {
  if (tray_view())
    tray_view()->SetVisible(visible);
  tray_icon_visible_ = visible;
}

tray::AccessibilityDetailedView* TrayAccessibility::CreateDetailedMenu() {
  return new tray::AccessibilityDetailedView(this);
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
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
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
        new tray::AccessibilityPopupView(request_popup_view_state_);
    detailed_popup_->Init();
    request_popup_view_state_ = A11Y_NONE;
    return detailed_popup_;
  } else {
    ShellPort::Get()->RecordUserMetricsAction(
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
    ShowDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else {
    if (detailed_popup_)
      detailed_popup_->GetWidget()->Close();
    if (detailed_menu_)
      detailed_menu_->GetWidget()->Close();
  }

  previous_accessibility_state_ = accessibility_state;
}

}  // namespace ash
