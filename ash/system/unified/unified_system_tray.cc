// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/web_notification/ash_popup_alignment_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/ui_controller.h"
#include "ui/message_center/ui_delegate.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace ash {

class UnifiedSystemTray::UiDelegate : public message_center::UiDelegate {
 public:
  explicit UiDelegate(UnifiedSystemTray* owner);
  ~UiDelegate() override;

  // message_center::UiDelegate:
  void OnMessageCenterContentsChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter(bool show_by_click) override;
  void HideMessageCenter() override;
  bool ShowNotifierSettings() override;

  message_center::UiController* ui_controller() { return ui_controller_.get(); }

 private:
  std::unique_ptr<message_center::UiController> ui_controller_;
  std::unique_ptr<AshPopupAlignmentDelegate> popup_alignment_delegate_;
  std::unique_ptr<message_center::MessagePopupCollection>
      message_popup_collection_;

  UnifiedSystemTray* const owner_;

  DISALLOW_COPY_AND_ASSIGN(UiDelegate);
};

UnifiedSystemTray::UiDelegate::UiDelegate(UnifiedSystemTray* owner)
    : owner_(owner) {
  ui_controller_ = std::make_unique<message_center::UiController>(this);
  popup_alignment_delegate_ =
      std::make_unique<AshPopupAlignmentDelegate>(owner->shelf());
  message_popup_collection_ =
      std::make_unique<message_center::MessagePopupCollection>(
          message_center::MessageCenter::Get(), ui_controller_.get(),
          popup_alignment_delegate_.get());
  display::Screen* screen = display::Screen::GetScreen();
  popup_alignment_delegate_->StartObserving(
      screen, screen->GetDisplayNearestWindow(
                  owner->shelf()->GetStatusAreaWidget()->GetNativeWindow()));
}

UnifiedSystemTray::UiDelegate::~UiDelegate() = default;

void UnifiedSystemTray::UiDelegate::OnMessageCenterContentsChanged() {
  // TODO(tetsui): Implement.
}

bool UnifiedSystemTray::UiDelegate::ShowPopups() {
  if (owner_->IsBubbleShown())
    return false;
  message_popup_collection_->DoUpdate();
  return true;
}

void UnifiedSystemTray::UiDelegate::HidePopups() {
  message_popup_collection_->MarkAllPopupsShown();
}

bool UnifiedSystemTray::UiDelegate::ShowMessageCenter(bool show_by_click) {
  if (owner_->IsBubbleShown())
    return false;

  owner_->ShowBubbleInternal();
  return true;
}

void UnifiedSystemTray::UiDelegate::HideMessageCenter() {
  owner_->HideBubbleInternal();
}

bool UnifiedSystemTray::UiDelegate::ShowNotifierSettings() {
  return false;
}

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      ui_delegate_(std::make_unique<UiDelegate>(this)),
      model_(std::make_unique<UnifiedSystemTrayModel>()) {
  // On the first step, features in the status area button are still provided by
  // TrayViews in SystemTray.
  // TODO(tetsui): Remove SystemTray from StatusAreaWidget and provide these
  // features from UnifiedSystemTray.
  SetVisible(false);
}

UnifiedSystemTray::~UnifiedSystemTray() = default;

bool UnifiedSystemTray::IsBubbleShown() const {
  return !!bubble_;
}

bool UnifiedSystemTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble(true /* show_by_click */);
  return true;
}

void UnifiedSystemTray::ShowBubble(bool show_by_click) {
  // ShowBubbleInternal will be called from UiDelegate.
  if (!bubble_)
    ui_delegate_->ui_controller()->ShowMessageCenterBubble(show_by_click);
}

void UnifiedSystemTray::CloseBubble() {
  // HideBubbleInternal will be called from UiDelegate.
  ui_delegate_->ui_controller()->HideMessageCenterBubble();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForTray() {
  // TODO(tetsui): Implement.
  return base::string16();
}

void UnifiedSystemTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble() {}

void UnifiedSystemTray::ShowBubbleInternal() {
  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this);
  // TODO(tetsui): Call its own SetIsActive. See the comment in the ctor.
  shelf()->GetStatusAreaWidget()->system_tray()->SetIsActive(true);
}

void UnifiedSystemTray::HideBubbleInternal() {
  bubble_.reset();
  // TODO(tetsui): Call its own SetIsActive. See the comment in the ctor.
  shelf()->GetStatusAreaWidget()->system_tray()->SetIsActive(false);
}

}  // namespace ash
