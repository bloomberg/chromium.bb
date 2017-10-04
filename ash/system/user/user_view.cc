// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_view.h"

#include <algorithm>
#include <utility>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/user/button_from_view.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/rounded_image_view.h"
#include "ash/system/user/user_card_view.h"
#include "base/memory/ptr_util.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
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
  SessionController* controller = Shell::Get()->session_controller();
  if (controller->IsUserSessionBlocked())
    return;

  // |user_index| must be in range (0, number_of_user). Note 0 is excluded
  // because it represents the active user and SwitchUser should not be called
  // for such case.
  DCHECK_GT(user_index, 0);
  DCHECK_LT(user_index, controller->NumberOfLoggedInUsers());

  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  controller->SwitchActiveUser(
      controller->GetUserSession(user_index)->user_info->account_id);
}

// Returns true when clicking the user card should show the user dropdown menu.
bool IsUserDropdownEnabled() {
  // Don't allow user add or switch when screen cast warning dialog is open.
  // See http://crrev.com/291276 and http://crbug.com/353170.
  if (ShellPort::Get()->IsSystemModalWindowOpen())
    return false;

  // Don't allow at login, lock or when adding a multi-profile user.
  SessionController* session = Shell::Get()->session_controller();
  if (session->IsUserSessionBlocked())
    return false;

  // Show if we can add or switch users.
  return session->GetAddUserPolicy() == AddUserSessionPolicy::ALLOWED ||
         session->NumberOfLoggedInUsers() > 1;
}

// Creates the view shown in the user switcher popup ("AddUserMenuOption").
views::View* CreateAddUserView(AddUserSessionPolicy policy) {
  auto* view = new views::View;
  const int icon_padding = (kMenuButtonSize - kMenuIconSize) / 2;
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(),
                           kTrayPopupLabelHorizontalPadding + icon_padding);
  layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
  view->SetLayoutManager(layout);
  view->SetBackground(views::CreateThemedSolidBackground(
      view, ui::NativeTheme::kColorId_BubbleBackground));

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
class UserDropdownWidgetContents : public views::View {
 public:
  explicit UserDropdownWidgetContents(const base::Closure& close_widget)
      : close_widget_(close_widget) {
    // Don't want to receive a mouse exit event when the cursor enters a child.
    set_notify_enter_exit_on_child(true);
  }

  ~UserDropdownWidgetContents() override {}

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

  DISALLOW_COPY_AND_ASSIGN(UserDropdownWidgetContents);
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

UserView::UserView(SystemTrayItem* owner, LoginStatus login) : owner_(owner) {
  CHECK_NE(LoginStatus::NOT_LOGGED_IN, login);
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  AddLogoutButton(login);
  AddUserCard(login);

  auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
  SetLayoutManager(layout);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->SetFlexForView(user_card_container_, 1);

  SetBorder(base::MakeUnique<ActiveUserBorder>());
}

UserView::~UserView() {
  HideUserDropdownWidget();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (user_dropdown_widget_)
    return add_user_enabled_ ? TrayUser::ACTIVE : TrayUser::ACTIVE_BUT_DISABLED;

  // If the container is the user card view itself, there's no ButtonFromView
  // wrapping it.
  if (user_card_container_ == user_card_view_)
    return TrayUser::SHOWN;

  return static_cast<ButtonFromView*>(user_card_container_)
                 ->is_hovered_for_test()
             ? TrayUser::HOVERED
             : TrayUser::SHOWN;
}

gfx::Rect UserView::GetBoundsInScreenOfUserButtonForTest() {
  return user_card_container_->GetBoundsInScreen();
}

int UserView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == logout_button_) {
    Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
    HideUserDropdownWidget();
    Shell::Get()->session_controller()->RequestSignOut();
  } else if (sender == user_card_container_ && IsUserDropdownEnabled()) {
    ToggleUserDropdownWidget();
  } else if (user_dropdown_widget_ &&
             sender->GetWidget() == user_dropdown_widget_.get()) {
    DCHECK_EQ(Shell::Get()->session_controller()->NumberOfLoggedInUsers(),
              sender->parent()->child_count() - 1);
    const int index_in_add_menu = sender->parent()->GetIndexOf(sender);
    // The last item is the "sign in another user" row.
    if (index_in_add_menu == sender->parent()->child_count() - 1) {
      MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
      Shell::Get()->session_controller()->ShowMultiProfileLogin();
    } else {
      const int user_index = index_in_add_menu;
      SwitchUser(user_index);
    }
    HideUserDropdownWidget();
    owner_->system_tray()->CloseBubble();
  } else {
    NOTREACHED();
  }
}

void UserView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  if (focused_now)
    HideUserDropdownWidget();
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
  DCHECK(!user_card_container_);
  DCHECK(!user_card_view_);
  user_card_view_ = new UserCardView(0);
  // The entry is clickable when the user menu can be opened.
  if (IsUserDropdownEnabled()) {
    user_card_container_ = new ButtonFromView(
        user_card_view_, this, TrayPopupInkDropStyle::INSET_BOUNDS);
  } else {
    user_card_container_ = user_card_view_;
  }
  AddChildViewAt(user_card_container_, 0);
}

void UserView::ToggleUserDropdownWidget() {
  if (user_dropdown_widget_) {
    HideUserDropdownWidget();
    return;
  }

  // Note: We do not need to install a global event handler to delete this
  // item since it will destroyed automatically before the menu / user menu item
  // gets destroyed..
  user_dropdown_widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_MENU;
  params.keep_on_top = true;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.name = "AddUserMenuOption";
  params.parent = GetWidget()->GetNativeWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_DragImageAndTooltipContainer);
  user_dropdown_widget_->Init(params);

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  const AddUserSessionPolicy add_user_policy =
      session_controller->GetAddUserPolicy();
  add_user_enabled_ = add_user_policy == AddUserSessionPolicy::ALLOWED;

  // Position the widget on top of the user card view (which is still in the
  // system menu). The top half of the widget will be transparent to allow
  // the active user to show through.
  gfx::Rect bounds = user_card_container_->GetBoundsInScreen();
  bounds.set_width(bounds.width() + kSeparatorWidth);
  int row_height = bounds.height();

  views::View* container = new UserDropdownWidgetContents(
      base::Bind(&UserView::HideUserDropdownWidget, base::Unretained(this)));
  views::View* add_user_view = CreateAddUserView(add_user_policy);
  const SkColor bg_color = add_user_view->background()->get_color();
  container->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidSidedBorder(0, 0, 0, kSeparatorWidth, bg_color),
      gfx::Insets(row_height, 0, 0, 0)));

  // Create the contents aside from the empty window through which the active
  // user is seen.
  views::View* user_dropdown_padding = new views::View();
  user_dropdown_padding->SetBorder(views::CreateSolidSidedBorder(
      kMenuSeparatorVerticalPadding - kSeparatorWidth, 0, 0, 0, bg_color));
  user_dropdown_padding->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));
  views::Separator* separator = new views::Separator();
  separator->SetPreferredHeight(kSeparatorWidth);
  separator->SetColor(
      color_utils::GetResultingPaintColor(kMenuSeparatorColor, bg_color));
  const int separator_horizontal_padding =
      (kTrayPopupItemMinStartWidth - kTrayItemSize) / 2;
  separator->SetBorder(
      views::CreateSolidSidedBorder(0, separator_horizontal_padding, 0,
                                    separator_horizontal_padding, bg_color));
  user_dropdown_padding->AddChildView(separator);

  // Add other logged in users.
  for (int i = 1; i < session_controller->NumberOfLoggedInUsers(); ++i) {
    user_dropdown_padding->AddChildView(new ButtonFromView(
        new UserCardView(i), this, TrayPopupInkDropStyle::INSET_BOUNDS));
  }

  // Add the "add user" option or the "can't add another user" message.
  if (add_user_enabled_) {
    auto* button = new ButtonFromView(add_user_view, this,
                                      TrayPopupInkDropStyle::INSET_BOUNDS);
    button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
    user_dropdown_padding->AddChildView(button);
  } else {
    user_dropdown_padding->AddChildView(add_user_view);
  }

  container->AddChildView(user_dropdown_padding);
  container->SetLayoutManager(new views::FillLayout());
  user_dropdown_widget_->SetContentsView(container);

  bounds.set_height(container->GetPreferredSize().height());
  user_dropdown_widget_->SetBounds(bounds);

  // Suppress the appearance of the collective capture icon while the dropdown
  // is open (the icon will appear in the specific user rows).
  user_card_view_->SetSuppressCaptureIcon(true);

  // Show the content.
  user_dropdown_widget_->SetAlwaysOnTop(true);
  user_dropdown_widget_->Show();

  // Install a listener to focus changes so that we can remove the card when
  // the focus gets changed. When called through the destruction of the bubble,
  // the FocusManager cannot be determined anymore and we remember it here.
  focus_manager_ = user_card_container_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);
}

void UserView::HideUserDropdownWidget() {
  if (!user_dropdown_widget_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
  if (user_card_container_->GetFocusManager())
    user_card_container_->GetFocusManager()->ClearFocus();
  user_card_view_->SetSuppressCaptureIcon(false);
  user_dropdown_widget_.reset();
}

}  // namespace tray
}  // namespace ash
