// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility_types.h"
#include "ash/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/public/cpp/message_center_switches.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/separator.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kNotificationId[] = "chrome://settings/accessibility";

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
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  uint32_t state = A11Y_NONE;
  if (delegate->IsSpokenFeedbackEnabled())
    state |= A11Y_SPOKEN_FEEDBACK;
  if (controller->IsHighContrastEnabled())
    state |= A11Y_HIGH_CONTRAST;
  if (delegate->IsMagnifierEnabled())
    state |= A11Y_SCREEN_MAGNIFIER;
  if (controller->IsLargeCursorEnabled())
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

// Returns notification icon based on the enabled accessibility state.
const gfx::VectorIcon& GetNotificationIcon(uint32_t enabled_accessibility) {
  if ((enabled_accessibility & A11Y_BRAILLE_DISPLAY_CONNECTED) &&
      (enabled_accessibility & A11Y_SPOKEN_FEEDBACK)) {
    return message_center::IsNewStyleNotificationEnabled()
               ? kNotificationAccessibilityIcon
               : kSystemMenuAccessibilityIcon;
  }
  if (enabled_accessibility & A11Y_BRAILLE_DISPLAY_CONNECTED)
    return kNotificationAccessibilityBrailleIcon;
  return message_center::IsNewStyleNotificationEnabled()
             ? kNotificationChromevoxIcon
             : kSystemMenuAccessibilityChromevoxIcon;
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
    set_id(VIEW_ID_ACCESSIBILITY_TRAY_ITEM);
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
// ash::tray::AccessibilityDetailedView

AccessibilityDetailedView::AccessibilityDetailedView(SystemTrayItem* owner)
    : TrayDetailsView(owner) {
  Reset();
  AppendAccessibilityList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);
  Layout();
}

void AccessibilityDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();

  spoken_feedback_enabled_ = delegate->IsSpokenFeedbackEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(spoken_feedback_view_,
                                            spoken_feedback_enabled_);

  high_contrast_enabled_ = controller->IsHighContrastEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(high_contrast_view_,
                                            high_contrast_enabled_);

  screen_magnifier_enabled_ = delegate->IsMagnifierEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(screen_magnifier_view_,
                                            screen_magnifier_enabled_);

  autoclick_enabled_ = delegate->IsAutoclickEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(autoclick_view_,
                                            autoclick_enabled_);

  virtual_keyboard_enabled_ = delegate->IsVirtualKeyboardEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(virtual_keyboard_view_,
                                            virtual_keyboard_enabled_);

  large_cursor_enabled_ = controller->IsLargeCursorEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(large_cursor_view_,
                                            large_cursor_enabled_);

  mono_audio_enabled_ = delegate->IsMonoAudioEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(mono_audio_view_,
                                            mono_audio_enabled_);

  caret_highlight_enabled_ = delegate->IsCaretHighlightEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(caret_highlight_view_,
                                            caret_highlight_enabled_);

  highlight_mouse_cursor_enabled_ = delegate->IsCursorHighlightEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(highlight_mouse_cursor_view_,
                                            highlight_mouse_cursor_enabled_);

  if (highlight_keyboard_focus_view_) {
    highlight_keyboard_focus_enabled_ = delegate->IsFocusHighlightEnabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(
        highlight_keyboard_focus_view_, highlight_keyboard_focus_enabled_);
  }

  sticky_keys_enabled_ = delegate->IsStickyKeysEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(sticky_keys_view_,
                                            sticky_keys_enabled_);

  tap_dragging_enabled_ = delegate->IsTapDraggingEnabled();
  TrayPopupUtils::UpdateCheckMarkVisibility(tap_dragging_view_,
                                            tap_dragging_enabled_);
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();

  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();

  spoken_feedback_enabled_ = delegate->IsSpokenFeedbackEnabled();
  spoken_feedback_view_ = AddScrollListCheckableItem(
      kSystemMenuAccessibilityChromevoxIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
      spoken_feedback_enabled_);

  high_contrast_enabled_ = controller->IsHighContrastEnabled();
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

  large_cursor_enabled_ = controller->IsLargeCursorEnabled();
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
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  using base::RecordAction;
  using base::UserMetricsAction;
  if (view == spoken_feedback_view_) {
    RecordAction(delegate->IsSpokenFeedbackEnabled()
                     ? UserMetricsAction("StatusArea_SpokenFeedbackDisabled")
                     : UserMetricsAction("StatusArea_SpokenFeedbackEnabled"));
    delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  } else if (view == high_contrast_view_) {
    bool new_state = !controller->IsHighContrastEnabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_HighContrastEnabled")
                     : UserMetricsAction("StatusArea_HighContrastDisabled"));
    controller->SetHighContrastEnabled(new_state);
  } else if (view == screen_magnifier_view_) {
    RecordAction(delegate->IsMagnifierEnabled()
                     ? UserMetricsAction("StatusArea_MagnifierDisabled")
                     : UserMetricsAction("StatusArea_MagnifierEnabled"));
    delegate->SetMagnifierEnabled(!delegate->IsMagnifierEnabled());
  } else if (large_cursor_view_ && view == large_cursor_view_) {
    bool new_state = !controller->IsLargeCursorEnabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_LargeCursorEnabled")
                     : UserMetricsAction("StatusArea_LargeCursorDisabled"));
    controller->SetLargeCursorEnabled(new_state);
  } else if (autoclick_view_ && view == autoclick_view_) {
    RecordAction(delegate->IsAutoclickEnabled()
                     ? UserMetricsAction("StatusArea_AutoClickDisabled")
                     : UserMetricsAction("StatusArea_AutoClickEnabled"));
    delegate->SetAutoclickEnabled(!delegate->IsAutoclickEnabled());
  } else if (virtual_keyboard_view_ && view == virtual_keyboard_view_) {
    RecordAction(delegate->IsVirtualKeyboardEnabled()
                     ? UserMetricsAction("StatusArea_VirtualKeyboardDisabled")
                     : UserMetricsAction("StatusArea_VirtualKeyboardEnabled"));
    delegate->SetVirtualKeyboardEnabled(!delegate->IsVirtualKeyboardEnabled());
  } else if (caret_highlight_view_ && view == caret_highlight_view_) {
    RecordAction(delegate->IsCaretHighlightEnabled()
                     ? UserMetricsAction("StatusArea_CaretHighlightDisabled")
                     : UserMetricsAction("StatusArea_CaretHighlightEnabled"));
    delegate->SetCaretHighlightEnabled(!delegate->IsCaretHighlightEnabled());
  } else if (mono_audio_view_ && view == mono_audio_view_) {
    RecordAction(delegate->IsMonoAudioEnabled()
                     ? UserMetricsAction("StatusArea_MonoAudioDisabled")
                     : UserMetricsAction("StatusArea_MonoAudioEnabled"));
    delegate->SetMonoAudioEnabled(!delegate->IsMonoAudioEnabled());
  } else if (highlight_mouse_cursor_view_ &&
             view == highlight_mouse_cursor_view_) {
    RecordAction(
        delegate->IsCursorHighlightEnabled()
            ? UserMetricsAction("StatusArea_HighlightMouseCursorDisabled")
            : UserMetricsAction("StatusArea_HighlightMouseCursorEnabled"));
    delegate->SetCursorHighlightEnabled(!delegate->IsCursorHighlightEnabled());
  } else if (highlight_keyboard_focus_view_ &&
             view == highlight_keyboard_focus_view_) {
    RecordAction(
        delegate->IsFocusHighlightEnabled()
            ? UserMetricsAction("StatusArea_HighlightKeyboardFocusDisabled")
            : UserMetricsAction("StatusArea_HighlightKeyboardFocusEnabled"));
    delegate->SetFocusHighlightEnabled(!delegate->IsFocusHighlightEnabled());
  } else if (sticky_keys_view_ && view == sticky_keys_view_) {
    RecordAction(delegate->IsStickyKeysEnabled()
                     ? UserMetricsAction("StatusArea_StickyKeysDisabled")
                     : UserMetricsAction("StatusArea_StickyKeysEnabled"));
    delegate->SetStickyKeysEnabled(!delegate->IsStickyKeysEnabled());
  } else if (tap_dragging_view_ && view == tap_dragging_view_) {
    RecordAction(delegate->IsTapDraggingEnabled()
                     ? UserMetricsAction("StatusArea_TapDraggingDisabled")
                     : UserMetricsAction("StatusArea_TapDraggingEnabled"));
    delegate->SetTapDraggingEnabled(!delegate->IsTapDraggingEnabled());
  }
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
    owner()->system_tray()->CloseBubble();
  }
}

void AccessibilityDetailedView::ShowHelp() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_controller()->ShowAccessibilityHelp();
    owner()->system_tray()->CloseBubble();
  }
}

}  // namespace tray

////////////////////////////////////////////////////////////////////////////////
// ash::TrayAccessibility

TrayAccessibility::TrayAccessibility(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayAccessibilityIcon,
                    UMA_ACCESSIBILITY),
      default_(nullptr),
      detailed_menu_(nullptr),
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
  CHECK(default_ == nullptr);

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
    return nullptr;

  CHECK(default_ == nullptr);
  default_ = new tray::DefaultAccessibilityView(this);

  return default_;
}

views::View* TrayAccessibility::CreateDetailedView(LoginStatus status) {
  CHECK(detailed_menu_ == nullptr);

  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_ACCESSIBILITY);
  detailed_menu_ = CreateDetailedMenu();
  return detailed_menu_;
}

void TrayAccessibility::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

void TrayAccessibility::OnDetailedViewDestroyed() {
  detailed_menu_ = nullptr;
}

void TrayAccessibility::UpdateAfterLoginStatusChange(LoginStatus status) {
  // Stores the a11y feature status on just entering the lock screen.
  if (login_ != LoginStatus::LOCKED && status == LoginStatus::LOCKED)
    show_a11y_menu_on_lock_screen_ = (GetAccessibilityState() != A11Y_NONE);

  login_ = status;
  SetTrayIconVisible(GetInitialVisibility());
}

void TrayAccessibility::OnAccessibilityStatusChanged(
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

  if (detailed_menu_)
    detailed_menu_->OnAccessibilityStatusChanged();

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kNotificationId, false /* by_user */);

  // Contains bits for spoken feedback and braille display connected currently
  // being enabled.
  uint32_t being_enabled =
      (accessibility_state & ~previous_accessibility_state_) &
      (A11Y_SPOKEN_FEEDBACK | A11Y_BRAILLE_DISPLAY_CONNECTED);
  previous_accessibility_state_ = accessibility_state;

  // Shows notification if |notify| is true and the spoken feedback is being
  // enabled or if a braille display is connected.
  if (notify != A11Y_NOTIFICATION_SHOW || being_enabled == A11Y_NONE)
    return;

  base::string16 text;
  base::string16 title;
  if (being_enabled & A11Y_BRAILLE_DISPLAY_CONNECTED &&
      being_enabled & A11Y_SPOKEN_FEEDBACK) {
    text =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED);
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_BRAILLE_ENABLED_TITLE);
  } else if (being_enabled & A11Y_BRAILLE_DISPLAY_CONNECTED) {
    text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BRAILLE_DISPLAY_CONNECTED);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_TITLE);
    text =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED);
  }

  std::unique_ptr<message_center::Notification> notification;
  if (message_center::IsNewStyleNotificationEnabled()) {
    notification = message_center::Notification::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title, text,
        gfx::Image(), base::string16(), GURL(),
        message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                   system_notifier::kNotifierAccessibility),
        message_center::RichNotificationData(), nullptr,
        GetNotificationIcon(being_enabled),
        message_center::SystemNotificationWarningLevel::NORMAL);
  } else {
    notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title, text,
        gfx::Image(gfx::CreateVectorIcon(GetNotificationIcon(being_enabled),
                                         kMenuIconSize, kMenuIconColor)),
        base::string16(), GURL(),
        message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                   system_notifier::kNotifierAccessibility),
        message_center::RichNotificationData(), nullptr);
  }
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
