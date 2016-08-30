// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Returns the max height of ImeListView.
int GetImeListViewMaxHeight() {
  const int max_items = 7;
  return GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT) * max_items;
}

// Shows language and input settings page.
void ShowIMESettings() {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SHOW_DETAILED);
  delegate->ShowIMESettings();
}

class ImeMenuLabel : public views::Label {
 public:
  ImeMenuLabel() {}
  ~ImeMenuLabel() override {}

  // views:Label:
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(kTrayImeIconSize, kTrayImeIconSize);
  }
  int GetHeightForWidth(int width) const override { return kTrayImeIconSize; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeMenuLabel);
};

TrayPopupHeaderButton* CreateImeMenuButton(views::ButtonListener* listener,
                                           int enabled_resource_id,
                                           int disabled_resource_id,
                                           int enabled_resource_id_hover,
                                           int disabled_resource_id_hover,
                                           int accessible_name_id,
                                           int message_id,
                                           int right_border) {
  TrayPopupHeaderButton* button =
      new TrayPopupHeaderButton(listener, enabled_resource_id,
                                disabled_resource_id, enabled_resource_id_hover,
                                disabled_resource_id_hover, accessible_name_id);
  button->SetTooltipText(l10n_util::GetStringUTF16(message_id));
  button->SetBorder(views::Border::CreateSolidSidedBorder(0, 0, 0, right_border,
                                                          kBorderDarkColor));
  return button;
}

// The view that contains buttons shown on the bottom of IME menu.
class ImeButtonsView : public views::View,
                       public views::ButtonListener,
                       public ViewClickListener {
 public:
  ImeButtonsView(bool show_emoji_button,
                 bool show_voice_button,
                 bool show_handwriting_button,
                 bool show_settings_button) {
    SetBorder(
        views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kBorderDarkColor));

    // If there's only one settings button, the bottom should be a label with
    // normal background. Otherwise, show button icons with header background.
    if (show_settings_button && !show_emoji_button && !show_voice_button &&
        !show_handwriting_button) {
      ShowOneSettingButton();
    } else {
      ShowButtons(show_emoji_button, show_voice_button, show_handwriting_button,
                  show_settings_button);
    }
    // TODO(azurewei): Add logic of switching between the two states when the
    // menu is shown.
  }

  ~ImeButtonsView() override {}

  // views::View:
  gfx::Size GetPreferredSize() const override {
    int size = GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT);
    return gfx::Size(size, size);
  }
  int GetHeightForWidth(int width) const override {
    return GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (emoji_button_ && sender == emoji_button_) {
      // TODO(azurewei): Opens emoji palette.
    } else if (voice_button_ && sender == voice_button_) {
      // TODO(azurewei): Brings virtual keyboard for emoji input.
    } else if (handwriting_button_ && sender == handwriting_button_) {
      // TODO(azurewei): Brings virtual keyboard for handwriting input.
    } else if (settings_button_ && sender == settings_button_) {
      ShowIMESettings();
    }
  }

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override {
    if (one_settings_button_view_ && sender == one_settings_button_view_) {
      ShowIMESettings();
    }
  }

 private:
  // Shows the UI of one settings button.
  void ShowOneSettingButton() {
    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    box_layout->SetDefaultFlex(1);
    SetLayoutManager(box_layout);
    one_settings_button_view_ = new HoverHighlightView(this);
    one_settings_button_view_->AddLabel(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_IME_SETTINGS),
        gfx::ALIGN_LEFT, false /* highlight */);
    AddChildView(one_settings_button_view_);
  }

  // Shows the UI of more than one buttons.
  void ShowButtons(bool show_emoji_button,
                   bool show_voice_button,
                   bool show_handwriting_button,
                   bool show_settings_button) {
    set_background(
        views::Background::CreateSolidBackground(kHeaderBackgroundColor));
    auto* box_layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, kTrayImeBottomRowPadding,
        kTrayImeBottomRowPadding, kTrayImeBottomRowPaddingBetweenItems);
    box_layout->SetDefaultFlex(1);
    SetLayoutManager(box_layout);

    if (show_emoji_button) {
      // TODO(azurewei): Creates the proper button with icons.
    }

    if (show_voice_button) {
      // TODO(azurewei): Creates the proper button with icons.
    }

    if (show_handwriting_button) {
      // TODO(azurewei): Creates the proper button with icons.
    }

    if (show_settings_button) {
      settings_button_ = CreateImeMenuButton(
          this, IDR_AURA_UBER_TRAY_SETTINGS, IDR_AURA_UBER_TRAY_SETTINGS,
          IDR_AURA_UBER_TRAY_SETTINGS, IDR_AURA_UBER_TRAY_SETTINGS,
          IDS_ASH_STATUS_TRAY_SETTINGS, IDS_ASH_STATUS_TRAY_SETTINGS, 0);
      AddChildView(settings_button_);
    }
  }

  TrayPopupHeaderButton* emoji_button_;
  TrayPopupHeaderButton* voice_button_;
  TrayPopupHeaderButton* handwriting_button_;
  TrayPopupHeaderButton* settings_button_;
  HoverHighlightView* one_settings_button_view_;

  DISALLOW_COPY_AND_ASSIGN(ImeButtonsView);
};

}  // namespace

ImeMenuTray::ImeMenuTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf), label_(new ImeMenuLabel()) {
  SetupLabelForTray(label_);
  tray_container()->AddChildView(label_);
  SetContentsBackground();
  WmShell::Get()->system_tray_notifier()->AddIMEObserver(this);
}

ImeMenuTray::~ImeMenuTray() {
  if (bubble_)
    bubble_->bubble_view()->reset_delegate();
  WmShell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
}

void ImeMenuTray::ShowImeMenuBubble() {
  views::TrayBubbleView::InitParams init_params(
      views::TrayBubbleView::ANCHOR_TYPE_TRAY, GetAnchorAlignment(),
      kTrayPopupMinWidth, kTrayPopupMaxWidth);
  init_params.first_item_has_no_margin = true;
  init_params.can_activate = true;
  init_params.close_on_deactivate = true;

  views::TrayBubbleView* bubble_view =
      views::TrayBubbleView::Create(tray_container(), this, &init_params);
  bubble_view->set_margins(gfx::Insets(7, 0, 0, 0));
  bubble_view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);

  // Adds IME list to the bubble.
  ime_list_view_ =
      new ImeListView(nullptr, false, ImeListView::SHOW_SINGLE_IME);
  if (ime_list_view_->scroll_content()->height() > GetImeListViewMaxHeight()) {
    ime_list_view_->scroller()->SetFixedSize(
        gfx::Size(kTrayPopupMaxWidth, GetImeListViewMaxHeight()));
  }
  bubble_view->AddChildView(ime_list_view_);

  // Adds IME buttons to the bubble if needed.
  LoginStatus login =
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
  if (login != LoginStatus::NOT_LOGGED_IN && login != LoginStatus::LOCKED &&
      !WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen())
    bubble_view->AddChildView(new ImeButtonsView(false, false, false, true));

  bubble_.reset(new TrayBubbleWrapper(this, bubble_view));
  SetDrawBackgroundAsActive(true);
}

bool ImeMenuTray::IsImeMenuBubbleShown() {
  return !!bubble_;
}

void ImeMenuTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  if (!MaterialDesignController::IsShelfMaterial())
    tray_container()->SetBorder(views::Border::NullBorder());
}

base::string16 ImeMenuTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::HideBubbleWithView(const views::TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HideImeMenuBubble();
}

void ImeMenuTray::ClickedOutsideBubble() {
  HideImeMenuBubble();
}

bool ImeMenuTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    HideImeMenuBubble();
  else
    ShowImeMenuBubble();
  return true;
}

void ImeMenuTray::OnIMERefresh() {
  UpdateTrayLabel();
  if (bubble_ && ime_list_view_) {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    ime_list_view_->Update(list, property_list, false,
                           ImeListView::SHOW_SINGLE_IME);
  }
}

void ImeMenuTray::OnIMEMenuActivationChanged(bool is_activated) {
  SetVisible(is_activated);
  if (is_activated)
    UpdateTrayLabel();
  else
    HideImeMenuBubble();
}

void ImeMenuTray::BubbleViewDestroyed() {
}

void ImeMenuTray::OnMouseEnteredView() {}

void ImeMenuTray::OnMouseExitedView() {}

base::string16 ImeMenuTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

gfx::Rect ImeMenuTray::GetAnchorRect(views::Widget* anchor_widget,
                                     AnchorType anchor_type,
                                     AnchorAlignment anchor_alignment) const {
  gfx::Rect rect =
      GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);

  if (IsHorizontalAlignment(shelf_alignment())) {
    // Moves the bubble to make its center aligns the center of the tray.
    int horizontal_offset =
        -rect.width() + (tray_container()->width() + kTrayPopupMinWidth) / 2;
    rect.Offset(horizontal_offset, 0);
  } else {
    // For vertical alignment, sets the bubble's bottom aligned to the bottom
    // of the icon for now.
    int vertical_offset = -rect.height() + tray_container()->height();
    rect.Offset(0, vertical_offset);
  }
  return rect;
}

void ImeMenuTray::OnBeforeBubbleWidgetInit(
    views::Widget* anchor_widget,
    views::Widget* bubble_widget,
    views::Widget::InitParams* params) const {
  // Place the bubble in the same root window as |anchor_widget|.
  WmLookup::Get()
      ->GetWindowForWidget(anchor_widget)
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          bubble_widget, kShellWindowId_SettingBubbleContainer, params);
}

void ImeMenuTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void ImeMenuTray::UpdateTrayLabel() {
  WmShell::Get()->system_tray_delegate()->GetCurrentIME(&current_ime_);

  // Updates the tray label based on the current input method.
  if (current_ime_.third_party)
    label_->SetText(current_ime_.short_name + base::UTF8ToUTF16("*"));
  else
    label_->SetText(current_ime_.short_name);
}

void ImeMenuTray::HideImeMenuBubble() {
  bubble_.reset();
  ime_list_view_ = nullptr;
  SetDrawBackgroundAsActive(false);
}

}  // namespace ash
