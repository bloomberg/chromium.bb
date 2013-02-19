// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/enterprise/tray_enterprise.h"

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace internal {

class EnterpriseDefaultView : public views::View {
 public:
  explicit EnterpriseDefaultView(ViewClickListener* click_listener);
  virtual ~EnterpriseDefaultView();
  void SetMessage(const string16& message);
 private:
  views::View* CreateChildView(const string16& message) const;

  ViewClickListener* click_listener_;
  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseDefaultView);
};

EnterpriseDefaultView::EnterpriseDefaultView(
    ViewClickListener* click_listener)
    : click_listener_(click_listener) {
  SetLayoutManager(new views::FillLayout());
  SetVisible(false);
}

EnterpriseDefaultView::~EnterpriseDefaultView() {
}

void EnterpriseDefaultView::SetMessage(const string16& message) {
  if (message_ == message)
    return;

  message_ = message;
  RemoveAllChildViews(true);
  if (!message_.empty()) {
    AddChildView(CreateChildView(message_));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

views::View* EnterpriseDefaultView::CreateChildView(
    const string16& message) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* icon =
      rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_ENTERPRISE_DARK);
  HoverHighlightView* child = new HoverHighlightView(click_listener_);
  child->AddIconAndLabel(*icon, message, gfx::Font::NORMAL);
  child->text_label()->SetMultiLine(true);
  child->text_label()->SetAllowCharacterBreak(true);
  child->set_border(views::Border::CreateEmptyBorder(0,
      kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingHorizontal));
  child->SetExpandable(true);
  child->SetVisible(true);
  return child;
}

TrayEnterprise::TrayEnterprise(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_view_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->
      AddEnterpriseDomainObserver(this);
}

TrayEnterprise::~TrayEnterprise() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveEnterpriseDomainObserver(this);
}

void TrayEnterprise::UpdateEnterpriseMessage() {
  string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetEnterpriseMessage();
  if (default_view_)
    default_view_->SetMessage(message);
}

views::View* TrayEnterprise::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_view_ == NULL);
  // For public accounts, enterprise ownership is indicated in the user details
  // instead.
  if (status == ash::user::LOGGED_IN_PUBLIC)
    return NULL;
  default_view_ = new EnterpriseDefaultView(this);
  UpdateEnterpriseMessage();
  return default_view_;
}

void TrayEnterprise::DestroyDefaultView() {
  default_view_ = NULL;
}

void TrayEnterprise::OnEnterpriseDomainChanged() {
  UpdateEnterpriseMessage();
}

void TrayEnterprise::ClickedOn(views::View* sender) {
  Shell::GetInstance()->system_tray_delegate()->ShowEnterpriseInfo();
}

} // namespace internal
} // namespace ash

