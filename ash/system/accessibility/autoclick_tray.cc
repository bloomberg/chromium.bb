// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/autoclick_tray_action_list_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Used for testing.
const int kActionsListId = 1;

}  // namespace

// The view that contains Autoclick menu title and settings button.
// TODO(katie): Refactor to share code with ImeTitleView.
class AutoclickTitleView : public views::View, public views::ButtonListener {
 public:
  explicit AutoclickTitleView(AutoclickTray* autoclick_tray)
      : autoclick_tray_(autoclick_tray) {
    SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(0, 0, kTraySeparatorWidth, 0,
                                      kMenuSeparatorColor),
        gfx::Insets(kMenuSeparatorVerticalPadding - kTraySeparatorWidth, 0)));
    auto box_layout =
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
    box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
    views::BoxLayout* layout_ptr = SetLayoutManager(std::move(box_layout));
    auto* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK));
    title_label->SetBorder(
        views::CreateEmptyBorder(0, kMenuEdgeEffectivePadding, 1, 0));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE,
                             false /* use_unified_theme */);
    style.SetupLabel(title_label);

    AddChildView(title_label);
    layout_ptr->SetFlexForView(title_label, 1);

    settings_button_ = new SystemMenuButton(
        this, kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_AUTOCLICK_SETTINGS);
    if (!TrayPopupUtils::CanOpenWebUISettings())
      settings_button_->SetEnabled(false);
    AddChildView(settings_button_);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, settings_button_);
    autoclick_tray_->OnSettingsPressed();
  }

  ~AutoclickTitleView() override = default;

 private:
  AutoclickTray* autoclick_tray_;
  SystemMenuButton* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTitleView);
};

AutoclickTray::AutoclickTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  SetInkDropMode(InkDropMode::ON);

  UpdateIconsForSession();
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(tray_image_);
  const int vertical_padding = (kTrayItemSize - tray_image_.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - tray_image_.width()) / 2;
  icon->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  icon->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK));
  // This transfers pointer ownership to the tray_container.
  tray_container()->AddChildView(icon);
  CheckStatusAndUpdateIcon();

  // Observe the accessibility controller state changes to know when autoclick
  // state is updated or when it is disabled/enabled.
  Shell::Get()->accessibility_controller()->AddObserver(this);

  // Observe the session state to know when the session type changes.
  Shell::Get()->session_controller()->AddObserver(this);
}

AutoclickTray::~AutoclickTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
}

base::string16 AutoclickTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_AUTOCLICK_TRAY_ACCESSIBLE_NAME);
}

bool AutoclickTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
    SetIsActive(false);
  } else {
    ShowBubble(true /* show by click */);
    SetIsActive(true);
    base::RecordAction(
        base::UserMetricsAction("Accessibility.Autoclick.TrayMenu.Open"));
  }
  return true;
}

void AutoclickTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void AutoclickTray::CloseBubble() {
  bubble_.reset();
}

void AutoclickTray::ShowBubble(bool show_by_click) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = GetBubbleAnchor();
  init_params.anchor_alignment = GetAnchorAlignment();
  init_params.min_width = kTrayMenuWidth;
  init_params.max_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.show_by_click = show_by_click;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());

  bubble_view->AddChildView(new AutoclickTitleView(this));

  AutoclickTrayActionListView* actions_list = new AutoclickTrayActionListView(
      this, Shell::Get()->accessibility_controller()->GetAutoclickEventType());
  actions_list->set_id(kActionsListId);
  bubble_view->AddChildView(actions_list);

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                true /* is_persistent */);
}

TrayBubbleView* AutoclickTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

base::string16 AutoclickTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_AUTOCLICK_TRAY_ACCESSIBLE_NAME);
}

bool AutoclickTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback_enabled();
}

void AutoclickTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void AutoclickTray::ClickedOutsideBubble() {
  CloseBubble();
  SetIsActive(false);
}

void AutoclickTray::OnAccessibilityStatusChanged() {
  CheckStatusAndUpdateIcon();
}

void AutoclickTray::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateIconsForSession();
  CheckStatusAndUpdateIcon();
}

bool AutoclickTray::ContainsPointInScreen(const gfx::Point& point) {
  if (GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

void AutoclickTray::OnSettingsPressed() {
  CloseBubble();
  SetIsActive(false);
  // TODO(katie): Try to jump to autoclick's specific settings.
  Shell::Get()->system_tray_model()->client_ptr()->ShowAccessibilitySettings();
  base::RecordAction(
      base::UserMetricsAction("Accessibility.Autoclick.TrayMenu.Settings"));
}

void AutoclickTray::OnEventTypePressed(mojom::AutoclickEventType type) {
  // When the user selects an autoclick event type option, close the bubble
  // view and update the autoclick controller's state.
  CloseBubble();
  SetIsActive(false);
  Shell::Get()->accessibility_controller()->SetAutoclickEventType(type);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosAutoclick.TrayMenu.ChangeAction",
                            type);
}

void AutoclickTray::UpdateIconsForSession() {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  SkColor color = TrayIconColor(session_state);

  // TODO(katie): Use autoclick asset when available.
  tray_image_ = gfx::CreateVectorIcon(kSystemTraySelectToSpeakNewuiIcon, color);
}

void AutoclickTray::CheckStatusAndUpdateIcon() {
  SetVisible(Shell::Get()->accessibility_controller()->autoclick_enabled());
}

}  // namespace ash
