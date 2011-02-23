// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_view.h"

#include <vector>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/notifications/balloon_view_host.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/background.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace {
// Menu commands
const int kNoopCommand = 0;
const int kRevokePermissionCommand = 1;

// Vertical margin between close button and menu button.
const int kControlButtonsMargin = 6;

// Top, Right margin for notification control view.
const int kControlViewTopMargin = 4;
const int kControlViewRightMargin = 6;
}  // namespace

namespace chromeos {

// NotificationControlView has close and menu buttons and
// overlays on top of renderer view.
class NotificationControlView : public views::View,
                                public views::ViewMenuDelegate,
                                public ui::SimpleMenuModel::Delegate,
                                public views::ButtonListener {
 public:
  explicit NotificationControlView(BalloonViewImpl* view)
      : balloon_view_(view),
        close_button_(NULL),
        options_menu_contents_(NULL),
        options_menu_menu_(NULL),
        options_menu_button_(NULL) {
    // TODO(oshima): make background transparent.
    set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    SkBitmap* close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    SkBitmap* close_button_m = rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK);
    SkBitmap* close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    SkBitmap* close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
    close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
    close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
    close_button_->SetBackground(
        SK_ColorBLACK, close_button_n, close_button_m);

    AddChildView(close_button_);

    options_menu_button_
        = new views::MenuButton(NULL, std::wstring(), this, false);
    options_menu_button_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    options_menu_button_->SetIcon(*rb.GetBitmapNamed(IDR_NOTIFICATION_MENU));
    options_menu_button_->set_border(NULL);

    options_menu_button_->set_icon_placement(views::TextButton::ICON_ON_RIGHT);
    AddChildView(options_menu_button_);

    // The control view will never be resized, so just layout once.
    gfx::Size options_size = options_menu_button_->GetPreferredSize();
    gfx::Size button_size = close_button_->GetPreferredSize();

    int height = std::max(options_size.height(), button_size.height());
    options_menu_button_->SetBounds(
        0, (height - options_size.height()) / 2,
        options_size.width(), options_size.height());

    close_button_->SetBounds(
        options_size.width() + kControlButtonsMargin,
        (height - button_size.height()) / 2,
        button_size.width(), button_size.height());

    SizeToPreferredSize();
  }

  virtual gfx::Size GetPreferredSize() {
    gfx::Rect total_bounds =
        close_button_->bounds().Union(options_menu_button_->bounds());
    return total_bounds.size();
  }

  // views::ViewMenuDelegate implements.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) {
    CreateOptionsMenu();
    options_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  }

  // views::ButtonListener implements.
  virtual void ButtonPressed(views::Button* sender, const views::Event&) {
    balloon_view_->Close(true);
  }

  // ui::SimpleMenuModel::Delegate impglements.
  virtual bool IsCommandIdChecked(int /* command_id */) const {
    // Nothing in the menu is checked.
    return false;
  }

  virtual bool IsCommandIdEnabled(int /* command_id */) const {
    // All the menu options are always enabled.
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int /* command_id */, ui::Accelerator* /* accelerator */) {
    // Currently no accelerators.
    return false;
  }

  virtual void ExecuteCommand(int command_id) {
    switch (command_id) {
      case kRevokePermissionCommand:
        balloon_view_->DenyPermission();
      default:
        NOTIMPLEMENTED();
    }
  }

 private:
  void CreateOptionsMenu() {
    if (options_menu_contents_.get())
      return;
    const string16 source_label_text = l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
        balloon_view_->balloon_->notification().display_source());
    const string16 label_text = l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
        balloon_view_->balloon_->notification().display_source());

    options_menu_contents_.reset(new ui::SimpleMenuModel(this));
    // TODO(oshima): Showing the source info in the menu for now.
    // Figure out where to show the source info.
    options_menu_contents_->AddItem(kNoopCommand, source_label_text);
    options_menu_contents_->AddItem(kRevokePermissionCommand, label_text);

    options_menu_menu_.reset(new views::Menu2(options_menu_contents_.get()));
  }

  BalloonViewImpl* balloon_view_;

  views::ImageButton* close_button_;

  // The options menu.
  scoped_ptr<ui::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* options_menu_button_;

  DISALLOW_COPY_AND_ASSIGN(NotificationControlView);
};

BalloonViewImpl::BalloonViewImpl(bool sticky, bool controls, bool web_ui)
    : balloon_(NULL),
      html_contents_(NULL),
      method_factory_(this),
      stale_(false),
      sticky_(sticky),
      controls_(controls),
      closed_(false),
      web_ui_(web_ui) {
  // This object is not to be deleted by the views hierarchy,
  // as it is owned by the balloon.
  set_parent_owned(false);
}

BalloonViewImpl::~BalloonViewImpl() {
  if (control_view_host_.get()) {
    control_view_host_->CloseNow();
  }
  if (html_contents_) {
    html_contents_->Shutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BallonViewImpl, BalloonView implementation.

void BalloonViewImpl::Show(Balloon* balloon) {
  balloon_ = balloon;
  html_contents_ = new BalloonViewHost(balloon);
  if (web_ui_)
    html_contents_->EnableWebUI();
  AddChildView(html_contents_->view());
  notification_registrar_.Add(this,
    NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon));
}

void BalloonViewImpl::Update() {
  stale_ = false;
  if (html_contents_->render_view_host())
    html_contents_->render_view_host()->NavigateToURL(
        balloon_->notification().content_url());
}

void BalloonViewImpl::Close(bool by_user) {
  closed_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BalloonViewImpl::DelayedClose, by_user));
}

gfx::Size BalloonViewImpl::GetSize() const {
  // Not used. The layout is managed by the Panel.
  return gfx::Size(0, 0);
}

BalloonHost* BalloonViewImpl::GetHost() const {
  return html_contents_;
}

void BalloonViewImpl::RepositionToBalloon() {
  // Not used. The layout is managed by the Panel.
}

////////////////////////////////////////////////////////////////////////////////
// views::View interface overrides.

void BalloonViewImpl::Layout() {
  gfx::Size size = balloon_->content_size();

  SetBounds(x(), y(), size.width(), size.height());

  html_contents_->view()->SetBounds(0, 0, size.width(), size.height());
  if (html_contents_->render_view_host()) {
    RenderWidgetHostView* view = html_contents_->render_view_host()->view();
    if (view)
      view->SetSize(size);
  }
}

void BalloonViewImpl::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  if (is_add && GetWidget() && !control_view_host_.get() && controls_) {
    control_view_host_.reset(
        new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD));
    control_view_host_->EnableDoubleBuffer(true);
    control_view_host_->Init(GetParentNativeView(), gfx::Rect());
    NotificationControlView* control = new NotificationControlView(this);
    control_view_host_->set_delete_on_destroy(false);
    control_view_host_->SetContentsView(control);
  }
  if (!is_add && this == child && control_view_host_.get() && controls_) {
    control_view_host_.release()->CloseNow();
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
// BalloonViewImpl public.

bool BalloonViewImpl::IsFor(const Notification& notification) const {
  return balloon_->notification().notification_id() ==
      notification.notification_id();
}

void BalloonViewImpl::Activated() {
  if (!control_view_host_.get())
    return;

  // Get the size of Control View.
  gfx::Size size =
      control_view_host_->GetRootView()->GetChildViewAt(0)->GetPreferredSize();
  control_view_host_->Show();
  control_view_host_->SetBounds(
      gfx::Rect(width() - size.width() - kControlViewRightMargin,
                kControlViewTopMargin,
                size.width(), size.height()));
}

void BalloonViewImpl::Deactivated() {
  if (control_view_host_.get()) {
    control_view_host_->Hide();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BalloonViewImpl private.

void BalloonViewImpl::DelayedClose(bool by_user) {
  html_contents_->Shutdown();
  html_contents_ = NULL;
  balloon_->OnClose(by_user);
}

void BalloonViewImpl::DenyPermission() {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  service->DenyPermission(balloon_->notification().origin_url());
}

gfx::NativeView BalloonViewImpl::GetParentNativeView() {
  RenderWidgetHostView* view = html_contents_->render_view_host()->view();
  DCHECK(view);
  return view->GetNativeView();
}

}  // namespace chromeos
