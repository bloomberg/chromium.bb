// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/user_view.h"

#include <algorithm>
#include <utility>

#include "ash/common/multi_profile_uma.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/user/button_from_view.h"
#include "ash/common/system/user/login_status.h"
#include "ash/common/system/user/rounded_image_view.h"
#include "ash/common/system/user/user_card_view.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace tray {

namespace {

// Switch to a user with the given |user_index|.
void SwitchUser(UserIndex user_index) {
  // Do not switch users when the log screen is presented.
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  if (delegate->IsUserSessionBlocked())
    return;

  DCHECK(user_index > 0);
  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  delegate->SwitchActiveUser(delegate->GetUserInfo(user_index)->GetAccountId());
}

bool IsMultiProfileSupportedAndUserActive() {
  return WmShell::Get()->delegate()->IsMultiProfilesEnabled() &&
         !WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked();
}

// Creates the view shown in the user switcher popup ("AddUserMenuOption").
views::View* CreateAddUserView(AddUserSessionPolicy policy,
                               views::ButtonListener* listener) {
  auto* view = new views::View;
  const int icon_padding = (kMenuButtonSize - kMenuIconSize) / 2;
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kTrayPopupLabelHorizontalPadding + icon_padding);
  layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
  view->SetLayoutManager(layout);
  view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  int message_id = 0;
  switch (policy) {
    case AddUserSessionPolicy::ALLOWED: {
      message_id = IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT;

      auto* icon = new views::ImageView();
      icon->SetImage(
          gfx::CreateVectorIcon(kSystemMenuNewUserIcon, kMenuIconColor));
      view->AddChildView(icon);
      break;
    }
    case AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
      message_id = IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER;
      break;
    case AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
      message_id = IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER;
      break;
    case AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
      message_id = IDS_ASH_STATUS_TRAY_MESSAGE_OUT_OF_USERS;
      break;
  }

  auto* command_label = new views::Label(l10n_util::GetStringUTF16(message_id));
  command_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  command_label->SetMultiLine(true);

  TrayPopupItemStyle label_style(
      TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  int vertical_padding = kMenuSeparatorVerticalPadding;
  if (policy != AddUserSessionPolicy::ALLOWED) {
    label_style.set_font_style(TrayPopupItemStyle::FontStyle::CAPTION);
    label_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
    vertical_padding += kMenuSeparatorVerticalPadding;
  }
  label_style.SetupLabel(command_label);
  view->AddChildView(command_label);
  view->SetBorder(views::CreateEmptyBorder(vertical_padding, icon_padding,
                                           vertical_padding,
                                           kTrayPopupLabelHorizontalPadding));
  if (policy == AddUserSessionPolicy::ALLOWED) {
    auto* button =
        new ButtonFromView(view, listener, TrayPopupInkDropStyle::INSET_BOUNDS);
    button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
    return button;
  }

  return view;
}

class UserViewMouseWatcherHost : public views::MouseWatcherHost {
 public:
  explicit UserViewMouseWatcherHost(const gfx::Rect& screen_area)
      : screen_area_(screen_area) {}
  ~UserViewMouseWatcherHost() override {}

  // Implementation of MouseWatcherHost.
  bool Contains(const gfx::Point& screen_point,
                views::MouseWatcherHost::MouseEventType type) override {
    return screen_area_.Contains(screen_point);
  }

 private:
  gfx::Rect screen_area_;

  DISALLOW_COPY_AND_ASSIGN(UserViewMouseWatcherHost);
};

// A view that acts as the contents of the widget that appears when clicking
// the active user. If the mouse exits this view or an otherwise unhandled
// click is detected, it will invoke a closure passed at construction time.
class AddUserWidgetContents : public views::View {
 public:
  explicit AddUserWidgetContents(const base::Closure& close_widget)
      : close_widget_(close_widget) {
    // Don't want to receive a mouse exit event when the cursor enters a child.
    set_notify_enter_exit_on_child(true);
  }

  ~AddUserWidgetContents() override {}

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    close_widget_.Run();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    close_widget_.Run();
  }
  void OnGestureEvent(ui::GestureEvent* event) override { close_widget_.Run(); }

 private:
  base::Closure close_widget_;

  DISALLOW_COPY_AND_ASSIGN(AddUserWidgetContents);
};

// This border reserves 4dp above and 8dp below and paints a horizontal
// separator 3dp below the host view.
class ActiveUserBorder : public views::Border {
 public:
  ActiveUserBorder() {}
  ~ActiveUserBorder() override {}

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    canvas->FillRect(
        gfx::Rect(
            0, view.height() - kMenuSeparatorVerticalPadding - kSeparatorWidth,
            view.width(), kSeparatorWidth),
        kMenuSeparatorColor);
  }

  gfx::Insets GetInsets() const override {
    return gfx::Insets(kMenuSeparatorVerticalPadding,
                       kMenuExtraMarginFromLeftEdge,
                       kMenuSeparatorVerticalPadding * 2, 0);
  }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveUserBorder);
};

}  // namespace

UserView::UserView(SystemTrayItem* owner, LoginStatus login, UserIndex index)
    : user_index_(index),
      user_card_view_(nullptr),
      owner_(owner),
      is_user_card_button_(false),
      logout_button_(nullptr),
      add_user_enabled_(true),
      focus_manager_(nullptr) {
  CHECK_NE(LoginStatus::NOT_LOGGED_IN, login);
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  // Note that only the current multiprofile user gets a button.
  if (IsActiveUser())
    AddLogoutButton(login);
  AddUserCard(login);

  auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  SetLayoutManager(layout);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->SetFlexForView(user_card_view_, 1);

  if (IsActiveUser())
    SetBorder(base::MakeUnique<ActiveUserBorder>());
}

UserView::~UserView() {
  RemoveAddUserMenuOption();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (add_menu_option_)
    return add_user_enabled_ ? TrayUser::ACTIVE : TrayUser::ACTIVE_BUT_DISABLED;

  if (!is_user_card_button_)
    return TrayUser::SHOWN;

  return static_cast<ButtonFromView*>(user_card_view_)->is_hovered_for_test()
             ? TrayUser::HOVERED
             : TrayUser::SHOWN;
}

gfx::Rect UserView::GetBoundsInScreenOfUserButtonForTest() {
  DCHECK(user_card_view_);
  return user_card_view_->GetBoundsInScreen();
}

bool UserView::IsActiveUser() const {
  return user_index_ == 0;
}

int UserView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == logout_button_) {
    WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
    RemoveAddUserMenuOption();
    WmShell::Get()->system_tray_controller()->SignOut();
  } else if (sender == user_card_view_ &&
             IsMultiProfileSupportedAndUserActive()) {
    if (IsActiveUser()) {
      ToggleAddUserMenuOption();
    } else {
      RemoveAddUserMenuOption();
      SwitchUser(user_index_);
      // Since the user list is about to change the system menu should get
      // closed.
      owner_->system_tray()->CloseSystemBubble();
    }
  } else if (add_menu_option_ &&
             sender->GetWidget() == add_menu_option_.get()) {
    RemoveAddUserMenuOption();
    // Let the user add another account to the session.
    MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
    WmShell::Get()->system_tray_delegate()->ShowUserLogin();
    owner_->system_tray()->CloseSystemBubble();
  } else {
    NOTREACHED();
  }
}

void UserView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  if (focused_now)
    RemoveAddUserMenuOption();
}

void UserView::OnDidChangeFocus(View* focused_before, View* focused_now) {
  // Nothing to do here.
}

void UserView::AddLogoutButton(LoginStatus login) {
  AddChildView(TrayPopupUtils::CreateVerticalSeparator());
  logout_button_ = TrayPopupUtils::CreateTrayPopupBorderlessButton(
      this, user::GetLocalizedSignOutStringForStatus(login, true));
  AddChildView(logout_button_);
}

void UserView::AddUserCard(LoginStatus login) {
  user_card_view_ = new UserCardView(login, -1, user_index_);
  // The entry is clickable when no system modal dialog is open and the multi
  // profile option is active.
  bool clickable = !WmShell::Get()->IsSystemModalWindowOpen() &&
                   IsMultiProfileSupportedAndUserActive();
  if (clickable) {
    views::View* contents_view = user_card_view_;
    auto* button =
        new ButtonFromView(contents_view, this,
                           IsActiveUser() ? TrayPopupInkDropStyle::INSET_BOUNDS
                                          : TrayPopupInkDropStyle::FILL_BOUNDS);
    user_card_view_ = button;
    is_user_card_button_ = true;
  }
  AddChildViewAt(user_card_view_, 0);
}

void UserView::ToggleAddUserMenuOption() {
  if (add_menu_option_) {
    RemoveAddUserMenuOption();
    return;
  }

  // Note: We do not need to install a global event handler to delete this
  // item since it will destroyed automatically before the menu / user menu item
  // gets destroyed..
  add_menu_option_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.name = "AddUserMenuOption";
  WmLookup::Get()
      ->GetWindowForWidget(GetWidget())
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          add_menu_option_.get(), kShellWindowId_DragImageAndTooltipContainer,
          &params);
  add_menu_option_->Init(params);

  const SessionStateDelegate* delegate =
      WmShell::Get()->GetSessionStateDelegate();
  const AddUserSessionPolicy add_user_policy =
      delegate->GetAddUserSessionPolicy();
  add_user_enabled_ = add_user_policy == AddUserSessionPolicy::ALLOWED;

  // Position the widget on top of the user card view (which is still in the
  // system menu). The top half of the widget will be transparent to allow
  // the active user to show through.
  gfx::Rect bounds = user_card_view_->GetBoundsInScreen();
  bounds.set_width(bounds.width() + kSeparatorWidth);
  int row_height = bounds.height();

  views::View* container = new AddUserWidgetContents(
      base::Bind(&UserView::RemoveAddUserMenuOption, base::Unretained(this)));
  container->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidSidedBorder(0, 0, 0, kSeparatorWidth, kBackgroundColor),
      gfx::Insets(row_height, 0, 0, 0)));
  views::View* add_user_padding = new views::View();
  add_user_padding->SetBorder(views::CreateSolidSidedBorder(
      kMenuSeparatorVerticalPadding, 0, 0, 0, kBackgroundColor));
  views::View* add_user_view = CreateAddUserView(add_user_policy, this);
  add_user_padding->AddChildView(add_user_view);
  add_user_padding->SetLayoutManager(new views::FillLayout());
  container->AddChildView(add_user_padding);
  container->SetLayoutManager(new views::FillLayout());
  add_menu_option_->SetContentsView(container);

  bounds.set_height(container->GetPreferredSize().height());
  add_menu_option_->SetBounds(bounds);

  // Show the content.
  add_menu_option_->SetAlwaysOnTop(true);
  add_menu_option_->Show();

  // Install a listener to focus changes so that we can remove the card when
  // the focus gets changed. When called through the destruction of the bubble,
  // the FocusManager cannot be determined anymore and we remember it here.
  focus_manager_ = user_card_view_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);
}

void UserView::RemoveAddUserMenuOption() {
  if (!add_menu_option_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
  if (user_card_view_->GetFocusManager())
    user_card_view_->GetFocusManager()->ClearFocus();
  add_menu_option_.reset();
}

}  // namespace tray
}  // namespace ash
