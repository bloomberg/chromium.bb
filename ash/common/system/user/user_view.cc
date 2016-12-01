// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/user_view.h"

#include <algorithm>
#include <utility>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/multi_profile_uma.h"
#include "ash/common/popup_message.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tray_popup_label_button_border.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/user/button_from_view.h"
#include "ash/common/system/user/login_status.h"
#include "ash/common/system/user/rounded_image_view.h"
#include "ash/common/system/user/user_card_view.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/painter.h"

namespace ash {
namespace tray {

namespace {

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

const int kPublicAccountLogoutButtonBorderImagesNormal[] = {
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
};

const int kPublicAccountLogoutButtonBorderImagesHovered[] = {
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_HOVER_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
};

// When a hover border is used, it is starting this many pixels before the icon
// position.
const int kTrayUserTileHoverBorderInset = 10;

// Offsetting the popup message relative to the tray menu.
const int kPopupMessageOffset = 25;

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
  DCHECK(UseMd());
  auto view = new views::View;
  const int icon_padding = (kMenuButtonSize - kMenuIconSize) / 2;
  auto layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kTrayPopupLabelHorizontalPadding + icon_padding);
  layout->set_minimum_cross_axis_size(
      GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT));
  view->SetLayoutManager(layout);
  view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  int message_id = 0;
  switch (policy) {
    case AddUserSessionPolicy::ALLOWED: {
      message_id = IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT;

      auto icon = new views::ImageView();
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

  auto command_label = new views::Label(l10n_util::GetStringUTF16(message_id));
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
    auto button =
        new ButtonFromView(view, listener, TrayPopupInkDropStyle::INSET_BOUNDS,
                           false, gfx::Insets());
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

// The menu item view which gets shown when the user clicks in multi profile
// mode onto the user item.
class AddUserView : public views::View {
 public:
  // The |owner| is the view for which this view gets created.
  AddUserView(ButtonFromView* owner);
  ~AddUserView() override;

  // Get the anchor view for a message.
  views::View* anchor() { return anchor_; }

 private:
  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;

  // Create the additional client content for this item.
  void AddContent();

  // This is the content we create and show.
  views::View* add_user_;

  // This is the owner view of this item.
  ButtonFromView* owner_;

  // The anchor view for targetted bubble messages.
  views::View* anchor_;

  DISALLOW_COPY_AND_ASSIGN(AddUserView);
};

AddUserView::AddUserView(ButtonFromView* owner)
    : add_user_(nullptr), owner_(owner), anchor_(nullptr) {
  DCHECK(!UseMd());
  AddContent();
  owner_->ForceBorderVisible(true);
}

AddUserView::~AddUserView() {
  owner_->ForceBorderVisible(false);
}

gfx::Size AddUserView::GetPreferredSize() const {
  return owner_->bounds().size();
}

void AddUserView::AddContent() {
  SetLayoutManager(new views::FillLayout());
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  add_user_ = new views::View;
  add_user_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
  AddChildViewAt(add_user_, 0);

  // Add the icon which is also the anchor for messages.
  add_user_->SetBorder(
      views::CreateEmptyBorder(0, kTrayUserTileHoverBorderInset, 0, 0));
  RoundedImageView* icon = new RoundedImageView(kTrayRoundedBorderRadius, true);
  anchor_ = icon;
  icon->SetImage(*ui::ResourceBundle::GetSharedInstance()
                      .GetImageNamed(IDR_AURA_UBER_TRAY_ADD_MULTIPROFILE_USER)
                      .ToImageSkia(),
                 gfx::Size(kTrayItemSize, kTrayItemSize));
  add_user_->AddChildView(icon);

  // Add the command text.
  views::Label* command_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  command_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  add_user_->AddChildView(command_label);
}

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
        kHorizontalSeparatorColor);
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
  if (!UseMd() && !index && login == LoginStatus::PUBLIC) {
    // Public user gets a yellow bg.
    set_background(views::Background::CreateSolidBackground(
        kPublicAccountBackgroundColor));
  }
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  // Note that only the current multiprofile user gets a button.
  if (IsActiveUser())
    AddLogoutButton(login);
  AddUserCard(login);

  if (UseMd()) {
    auto layout = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    SetLayoutManager(layout);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    layout->SetFlexForView(user_card_view_, 1);

    if (IsActiveUser())
      SetBorder(base::MakeUnique<ActiveUserBorder>());
  }
}

UserView::~UserView() {
  RemoveAddUserMenuOption();
}

void UserView::MouseMovedOutOfHost() {
  DCHECK(!UseMd());
  RemoveAddUserMenuOption();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (add_menu_option_.get()) {
    return add_user_enabled_ ? TrayUser::ACTIVE : TrayUser::ACTIVE_BUT_DISABLED;
  }

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

gfx::Size UserView::GetPreferredSize() const {
  // MD uses a layout manager.
  if (UseMd())
    return View::GetPreferredSize();

  // The width is more or less ignored (set by other rows in the system menu).
  gfx::Size size = user_card_view_->GetPreferredSize();
  if (logout_button_)
    size.SetToMax(logout_button_->GetPreferredSize());
  // Only the active user panel will be forced to a certain height.
  if (IsActiveUser()) {
    size.set_height(std::max(
        size.height(),
        GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT) + GetInsets().height()));
  }
  return size;
}

int UserView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void UserView::Layout() {
  // MD uses a layout manager.
  if (UseMd())
    return views::View::Layout();

  gfx::Rect contents_area(GetContentsBounds());
  if (logout_button_) {
    // Give the logout button the space it requests.
    gfx::Rect logout_area = contents_area;
    logout_area.ClampToCenteredSize(logout_button_->GetPreferredSize());
    logout_area.set_x(contents_area.right() - logout_area.width());

    // Give the remaining space to the user card.
    gfx::Rect user_card_area = contents_area;
    int remaining_width = contents_area.width() - logout_area.width();
    if (IsMultiProfileSupportedAndUserActive()) {
      // In multiprofile case |user_card_view_| and |logout_button_| have to
      // have the same height.
      int y = std::min(user_card_area.y(), logout_area.y());
      int height = std::max(user_card_area.height(), logout_area.height());
      logout_area.set_y(y);
      logout_area.set_height(height);
      user_card_area.set_y(y);
      user_card_area.set_height(height);

      // In multiprofile mode we have also to increase the size of the card by
      // the size of the border to make it overlap with the logout button.
      user_card_area.set_width(std::max(0, remaining_width + 1));

      // To make the logout button symmetrical with the user card we also make
      // the button longer by the same size the hover area in front of the icon
      // got inset.
      logout_area.set_width(logout_area.width() +
                            kTrayUserTileHoverBorderInset);
    } else {
      // In all other modes we have to make sure that there is enough spacing
      // between the two.
      remaining_width -= kTrayPopupPaddingBetweenItems;
    }
    user_card_area.set_width(remaining_width);
    user_card_view_->SetBoundsRect(user_card_area);
    logout_button_->SetBoundsRect(logout_area);
  } else {
    user_card_view_->SetBoundsRect(contents_area);
  }
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
  } else if (add_menu_option_.get() &&
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
  const base::string16 title =
      user::GetLocalizedSignOutStringForStatus(login, true);
  auto* logout_button =
      TrayPopupUtils::CreateTrayPopupBorderlessButton(this, title);
  logout_button->SetAccessibleName(title);
  logout_button_ = logout_button;
  if (UseMd()) {
    AddChildView(TrayPopupUtils::CreateVerticalSeparator());
  } else if (login == LoginStatus::PUBLIC) {
    // In public account mode, the logout button border has a custom color.
    std::unique_ptr<TrayPopupLabelButtonBorder> border(
        new TrayPopupLabelButtonBorder());
    border->SetPainter(false, views::Button::STATE_NORMAL,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesNormal));
    border->SetPainter(false, views::Button::STATE_HOVERED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    border->SetPainter(false, views::Button::STATE_PRESSED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    logout_button_->SetBorder(std::move(border));
  }
  AddChildView(logout_button_);
}

void UserView::AddUserCard(LoginStatus login) {
  if (UseMd())
    return AddUserCardMd(login);

  // Add padding around the panel.
  const int kSidePadding = kTrayPopupPaddingHorizontal;
  SetBorder(views::CreateEmptyBorder(
      kTrayPopupUserCardVerticalPadding, kSidePadding,
      kTrayPopupUserCardVerticalPadding, kSidePadding));

  views::TrayBubbleView* bubble_view =
      owner_->system_tray()->GetSystemBubble()->bubble_view();
  int max_card_width = bubble_view->GetMaximumSize().width() -
                       (2 * kSidePadding + kTrayPopupPaddingBetweenItems);
  if (logout_button_)
    max_card_width -= logout_button_->GetPreferredSize().width();
  user_card_view_ = new UserCardView(login, max_card_width, user_index_);
  // The entry is clickable when no system modal dialog is open and the multi
  // profile option is active.
  bool clickable = !WmShell::Get()->IsSystemModalWindowOpen() &&
                   IsMultiProfileSupportedAndUserActive();
  if (clickable) {
    // To allow the border to start before the icon, reduce the size before and
    // add an inset to the icon to get the spacing.
    if (IsActiveUser()) {
      SetBorder(views::CreateEmptyBorder(
          kTrayPopupUserCardVerticalPadding,
          kSidePadding - kTrayUserTileHoverBorderInset,
          kTrayPopupUserCardVerticalPadding, kSidePadding));
      user_card_view_->SetBorder(
          views::CreateEmptyBorder(0, kTrayUserTileHoverBorderInset, 0, 0));
    }
    gfx::Insets insets = gfx::Insets(1, 1, 1, 1);
    views::View* contents_view = user_card_view_;
    if (!IsActiveUser()) {
      // Since the activation border needs to be drawn around the tile, we
      // have to put the tile into another view which fills the menu panel,
      // but keeping the offsets of the content.
      contents_view = new views::View();
      contents_view->SetBorder(views::CreateEmptyBorder(
          kTrayPopupUserCardVerticalPadding, kSidePadding,
          kTrayPopupUserCardVerticalPadding, kSidePadding));
      contents_view->SetLayoutManager(new views::FillLayout());
      SetBorder(nullptr);
      contents_view->AddChildView(user_card_view_);
      insets = gfx::Insets(1, 1, 1, 3);
    }
    auto* button = new ButtonFromView(contents_view, this,
                                      // This parameter is ignored in non-md.
                                      TrayPopupInkDropStyle::FILL_BOUNDS,
                                      IsActiveUser(), insets);
    user_card_view_ = button;
    is_user_card_button_ = true;
  }
  AddChildViewAt(user_card_view_, 0);
  // Card for supervised user can consume more space than currently
  // available. In that case we should increase system bubble's width.
  if (login == LoginStatus::PUBLIC)
    bubble_view->SetWidth(GetPreferredSize().width());
}

void UserView::AddUserCardMd(LoginStatus login) {
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
                                          : TrayPopupInkDropStyle::FILL_BOUNDS,
                           false, gfx::Insets());
    user_card_view_ = button;
    is_user_card_button_ = true;
  }
  AddChildViewAt(user_card_view_, 0);
}

void UserView::ToggleAddUserMenuOption() {
  if (add_menu_option_.get()) {
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
  if (!UseMd())
    params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
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

  if (UseMd()) {
    // Position the widget on top of the user card view (which is still in the
    // system menu). The top half of the widget will be transparent to allow
    // the active user to show through.
    gfx::Rect bounds = user_card_view_->GetBoundsInScreen();
    bounds.set_width(bounds.width() + kSeparatorWidth);
    int row_height = bounds.height();

    views::View* container = new AddUserWidgetContents(
        base::Bind(&UserView::RemoveAddUserMenuOption, base::Unretained(this)));
    container->SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(0, 0, 0, kSeparatorWidth,
                                      kBackgroundColor),
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

    // We activate the entry automatically if invoked with focus.
    if (add_user_enabled_ && user_card_view_->HasFocus()) {
      add_user_view->GetFocusManager()->SetFocusedView(add_user_view);
      user_card_view_->GetFocusManager()->SetFocusedView(add_user_view);
    }
  } else {
    AddUserView* add_user_view =
        new AddUserView(static_cast<ButtonFromView*>(user_card_view_));
    ButtonFromView* button = new ButtonFromView(
        add_user_view, add_user_enabled_ ? this : nullptr,
        // Ignored in non-md.
        TrayPopupInkDropStyle::INSET_BOUNDS, add_user_enabled_, gfx::Insets(1));
    button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
    button->ForceBorderVisible(true);

    add_menu_option_->SetOpacity(1.f);
    add_menu_option_->SetContentsView(button);
    // Position it below our user card.
    gfx::Rect bounds = user_card_view_->GetBoundsInScreen();
    bounds.set_y(bounds.y() + bounds.height());
    add_menu_option_->SetBounds(bounds);

    // Show the content.
    add_menu_option_->SetAlwaysOnTop(true);
    add_menu_option_->Show();

    if (add_user_enabled_) {
      // We activate the entry automatically if invoked with focus.
      if (user_card_view_->HasFocus()) {
        button->GetFocusManager()->SetFocusedView(button);
        user_card_view_->GetFocusManager()->SetFocusedView(button);
      }
    } else {
      ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
      int message_id = 0;
      switch (add_user_policy) {
        case AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
          message_id = IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER;
          break;
        case AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
          message_id = IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER;
          break;
        case AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
          message_id = IDS_ASH_STATUS_TRAY_MESSAGE_OUT_OF_USERS;
          break;
        default:
          NOTREACHED() << "Unknown adding user policy "
                       << static_cast<int>(add_user_policy);
      }

      popup_message_.reset(new PopupMessage(
          bundle.GetLocalizedString(
              IDS_ASH_STATUS_TRAY_CAPTION_CANNOT_ADD_USER),
          bundle.GetLocalizedString(message_id), PopupMessage::ICON_WARNING,
          add_user_view->anchor(), views::BubbleBorder::TOP_LEFT,
          gfx::Size(parent()->bounds().width() - kPopupMessageOffset, 0),
          2 * kPopupMessageOffset));
    }
    // Find the screen area which encloses both elements and sets then a mouse
    // watcher which will close the "menu".
    gfx::Rect area = user_card_view_->GetBoundsInScreen();
    area.set_height(2 * area.height());
    mouse_watcher_.reset(
        new views::MouseWatcher(new UserViewMouseWatcherHost(area), this));
    mouse_watcher_->Start();
  }
  // Install a listener to focus changes so that we can remove the card when
  // the focus gets changed. When called through the destruction of the bubble,
  // the FocusManager cannot be determined anymore and we remember it here.
  focus_manager_ = user_card_view_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);
}

void UserView::RemoveAddUserMenuOption() {
  if (!add_menu_option_.get())
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
  if (user_card_view_->GetFocusManager())
    user_card_view_->GetFocusManager()->ClearFocus();
  popup_message_.reset();
  mouse_watcher_.reset();
  add_menu_option_.reset();
}

}  // namespace tray
}  // namespace ash
