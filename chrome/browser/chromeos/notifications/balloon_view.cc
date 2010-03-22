// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_view.h"

#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/views/notifications/balloon_view_host.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/widget/root_view.h"

namespace {
// Menu commands
const int kRevokePermissionCommand = 0;

}  // namespace

namespace chromeos {

BalloonViewImpl::BalloonViewImpl(bool sticky, bool controls)
    : balloon_(NULL),
      html_contents_(NULL),
      method_factory_(this),
      close_button_(NULL),
      options_menu_contents_(NULL),
      options_menu_menu_(NULL),
      options_menu_button_(NULL),
      stale_(false),
      sticky_(sticky),
      controls_(controls) {
  // This object is not to be deleted by the views hierarchy,
  // as it is owned by the balloon.
  set_parent_owned(false);
}

BalloonViewImpl::~BalloonViewImpl() {
  if (html_contents_) {
    html_contents_->Shutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BallonViewImpl, BalloonView implementation.

void BalloonViewImpl::Show(Balloon* balloon) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const std::wstring source_label_text = l10n_util::GetStringF(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      balloon->notification().display_source());
  const std::wstring options_text =
      l10n_util::GetString(IDS_NOTIFICATION_OPTIONS_MENU_LABEL);
  const std::wstring dismiss_text =
      l10n_util::GetString(IDS_NOTIFICATION_BALLOON_DISMISS_LABEL);
  balloon_ = balloon;

  html_contents_ = new BalloonViewHost(balloon);
  AddChildView(html_contents_);
  if (controls_) {
    close_button_ = new views::TextButton(this, dismiss_text);
    close_button_->SetIcon(*rb.GetBitmapNamed(IDR_BALLOON_CLOSE));
    close_button_->SetHoverIcon(*rb.GetBitmapNamed(IDR_BALLOON_CLOSE_HOVER));
    close_button_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    close_button_->SetEnabledColor(SK_ColorWHITE);
    close_button_->SetHoverColor(SK_ColorDKGRAY);
    close_button_->set_alignment(views::TextButton::ALIGN_CENTER);
    close_button_->set_icon_placement(views::TextButton::ICON_ON_RIGHT);
    AddChildView(close_button_);

    options_menu_button_
        = new views::MenuButton(NULL, options_text, this, false);
    options_menu_button_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    options_menu_button_->SetIcon(
        *rb.GetBitmapNamed(IDR_BALLOON_OPTIONS_ARROW));
    options_menu_button_->SetHoverIcon(
        *rb.GetBitmapNamed(IDR_BALLOON_OPTIONS_ARROW_HOVER));
    options_menu_button_->set_alignment(views::TextButton::ALIGN_CENTER);
    options_menu_button_->set_icon_placement(views::TextButton::ICON_ON_RIGHT);
    options_menu_button_->SetEnabledColor(SK_ColorWHITE);
    options_menu_button_->SetHoverColor(SK_ColorDKGRAY);
    AddChildView(options_menu_button_);

    source_label_ = new views::Label(source_label_text);
    source_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    source_label_->SetColor(SK_ColorWHITE);
    source_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(source_label_);
  }
  notification_registrar_.Add(this,
    NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon));
}

void BalloonViewImpl::Update() {
  stale_ = false;
  html_contents_->render_view_host()->NavigateToURL(
      balloon_->notification().content_url());
}

void BalloonViewImpl::Close(bool by_user) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BalloonViewImpl::DelayedClose, by_user));
}

gfx::Size BalloonViewImpl::GetSize() const {
  // Not used. The layout is managed by the Panel.
  return gfx::Size(0, 0);
}

void BalloonViewImpl::RepositionToBalloon() {
  // Not used. The layout is managed by the Panel.
}

////////////////////////////////////////////////////////////////////////////////
// views::View interface overrides.

void BalloonViewImpl::Layout() {
  gfx::Size button_size;
  if (close_button_) {
    button_size = close_button_->GetPreferredSize();
  }

  SetBounds(x(), y(),
            balloon_->content_size().width(),
            balloon_->content_size().height() +
            button_size.height());
  int x = width() - button_size.width();
  int y = height() - button_size.height();

  html_contents_->SetBounds(0, 0, width(), y);
  if (html_contents_->render_view_host()) {
    RenderWidgetHostView* view = html_contents_->render_view_host()->view();
    if (view)
      view->SetSize(gfx::Size(width(), y));
  }
  if (controls_) {
    close_button_->SetBounds(x, y, button_size.width(), button_size.height());
    x -= close_button_->GetPreferredSize().width();
    options_menu_button_->SetBounds(
        x, y, button_size.width(), button_size.height());
    source_label_->SetBounds(0, y, x, button_size.height());
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.

void BalloonViewImpl::RunMenu(views::View* source, const gfx::Point& pt) {
  CreateOptionsMenu();
  options_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// views::Button implementation.

void BalloonViewImpl::ButtonPressed(views::Button* sender,
                                    const views::Event&) {
  Close(true);
}

////////////////////////////////////////////////////////////////////////////////
// menus::SimpleMenuModel::Delegate implementation.

bool BalloonViewImpl::IsCommandIdChecked(int /* command_id */) const {
  // Nothing in the menu is checked.
  return false;
}

bool BalloonViewImpl::IsCommandIdEnabled(int /* command_id */) const {
  // All the menu options are always enabled.
  return true;
}

bool BalloonViewImpl::GetAcceleratorForCommandId(
    int /* command_id */, menus::Accelerator* /* accelerator */) {
  // Currently no accelerators.
  return false;
}

void BalloonViewImpl::ExecuteCommand(int command_id) {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  switch (command_id) {
    case kRevokePermissionCommand:
      service->DenyPermission(balloon_->notification().origin_url());
      break;
    default:
      NOTIMPLEMENTED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationObserver overrides.

void BalloonViewImpl::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type != NotificationType::NOTIFY_BALLOON_DISCONNECTED) {
    NOTREACHED();
    return;
  }

  // If the renderer process attached to this balloon is disconnected
  // (e.g., because of a crash), we want to close the balloon.
  notification_registrar_.Remove(this,
      NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon_));
  Close(false);
}

////////////////////////////////////////////////////////////////////////////////
// BalloonViewImpl private.

void BalloonViewImpl::CreateOptionsMenu() {
  if (options_menu_contents_.get())
    return;

  const string16 label_text = WideToUTF16Hack(l10n_util::GetStringF(
      IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
      this->balloon_->notification().display_source()));

  options_menu_contents_.reset(new menus::SimpleMenuModel(this));
  options_menu_contents_->AddItem(kRevokePermissionCommand, label_text);

  options_menu_menu_.reset(new views::Menu2(options_menu_contents_.get()));
}

void BalloonViewImpl::DelayedClose(bool by_user) {
  html_contents_->Shutdown();
  html_contents_ = NULL;
  balloon_->OnClose(by_user);
}

}  // namespace chromeos
