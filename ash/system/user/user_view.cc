// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_view.h"

#include <algorithm>

#include "ash/multi_profile_uma.h"
#include "ash/popup_message.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "ash/system/tray/tray_popup_label_button_border.h"
#include "ash/system/user/button_from_view.h"
#include "ash/system/user/config.h"
#include "ash/system/user/rounded_image_view.h"
#include "ash/system/user/user_card_view.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace tray {

namespace {

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
void SwitchUser(ash::MultiProfileIndex user_index) {
  // Do not switch users when the log screen is presented.
  if (ash::Shell::GetInstance()
          ->session_state_delegate()
          ->IsUserSessionBlocked())
    return;

  DCHECK(user_index > 0);
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  ash::MultiProfileUMA::RecordSwitchActiveUser(
      ash::MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  delegate->SwitchActiveUser(delegate->GetUserInfo(user_index)->GetUserID());
}

class LogoutButton : public TrayPopupLabelButton {
 public:
  // If |placeholder| is true, button is used as placeholder. That means that
  // button is inactive and is not painted, but consume the same ammount of
  // space, as if it was painted.
  LogoutButton(views::ButtonListener* listener,
               const base::string16& text,
               bool placeholder)
      : TrayPopupLabelButton(listener, text), placeholder_(placeholder) {
    SetEnabled(!placeholder_);
  }

  virtual ~LogoutButton() {}

 private:
  virtual void Paint(gfx::Canvas* canvas,
                     const views::CullSet& cull_set) OVERRIDE {
    // Just skip paint if this button used as a placeholder.
    if (!placeholder_)
      TrayPopupLabelButton::Paint(canvas, cull_set);
  }

  bool placeholder_;
  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

class UserViewMouseWatcherHost : public views::MouseWatcherHost {
 public:
  explicit UserViewMouseWatcherHost(const gfx::Rect& screen_area)
      : screen_area_(screen_area) {}
  virtual ~UserViewMouseWatcherHost() {}

  // Implementation of MouseWatcherHost.
  virtual bool Contains(const gfx::Point& screen_point,
                        views::MouseWatcherHost::MouseEventType type) OVERRIDE {
    return screen_area_.Contains(screen_point);
  }

 private:
  gfx::Rect screen_area_;

  DISALLOW_COPY_AND_ASSIGN(UserViewMouseWatcherHost);
};

// The menu item view which gets shown when the user clicks in multi profile
// mode onto the user item.
class AddUserView : public views::View {
 public:
  // The |owner| is the view for which this view gets created.
  AddUserView(ButtonFromView* owner);
  virtual ~AddUserView();

  // Get the anchor view for a message.
  views::View* anchor() { return anchor_; }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

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
    : add_user_(NULL), owner_(owner), anchor_(NULL) {
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
  add_user_->SetBorder(views::Border::CreateEmptyBorder(
      0, kTrayUserTileHoverBorderInset, 0, 0));

  add_user_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
  AddChildViewAt(add_user_, 0);

  // Add the [+] icon which is also the anchor for messages.
  RoundedImageView* icon = new RoundedImageView(kTrayAvatarCornerRadius, true);
  anchor_ = icon;
  icon->SetImage(*ui::ResourceBundle::GetSharedInstance()
                      .GetImageNamed(IDR_AURA_UBER_TRAY_ADD_MULTIPROFILE_USER)
                      .ToImageSkia(),
                 gfx::Size(kTrayAvatarSize, kTrayAvatarSize));
  add_user_->AddChildView(icon);

  // Add the command text.
  views::Label* command_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  command_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  add_user_->AddChildView(command_label);
}

}  // namespace

UserView::UserView(SystemTrayItem* owner,
                   user::LoginStatus login,
                   MultiProfileIndex index,
                   bool for_detailed_view)
    : multiprofile_index_(index),
      user_card_view_(NULL),
      owner_(owner),
      is_user_card_button_(false),
      logout_button_(NULL),
      add_user_disabled_(false),
      for_detailed_view_(for_detailed_view),
      focus_manager_(NULL) {
  CHECK_NE(user::LOGGED_IN_NONE, login);
  if (!index) {
    // Only the logged in user will have a background. All other users will have
    // to allow the TrayPopupContainer highlighting the menu line.
    set_background(views::Background::CreateSolidBackground(
        login == user::LOGGED_IN_PUBLIC ? kPublicAccountBackgroundColor
                                        : kBackgroundColor));
  }
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  // Note that only the current multiprofile user gets a button.
  if (!multiprofile_index_)
    AddLogoutButton(login);
  AddUserCard(login);
}

UserView::~UserView() {
  RemoveAddUserMenuOption();
}

void UserView::MouseMovedOutOfHost() {
  RemoveAddUserMenuOption();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (add_menu_option_.get()) {
    return add_user_disabled_ ? TrayUser::ACTIVE_BUT_DISABLED
                              : TrayUser::ACTIVE;
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

gfx::Size UserView::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  // Only the active user panel will be forced to a certain height.
  if (!multiprofile_index_) {
    size.set_height(
        std::max(size.height(), kTrayPopupItemHeight + GetInsets().height()));
  }
  return size;
}

int UserView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void UserView::Layout() {
  gfx::Rect contents_area(GetContentsBounds());
  if (user_card_view_ && logout_button_) {
    // Give the logout button the space it requests.
    gfx::Rect logout_area = contents_area;
    logout_area.ClampToCenteredSize(logout_button_->GetPreferredSize());
    logout_area.set_x(contents_area.right() - logout_area.width());

    // Give the remaining space to the user card.
    gfx::Rect user_card_area = contents_area;
    int remaining_width = contents_area.width() - logout_area.width();
    if (IsMultiProfileSupportedAndUserActive() ||
        IsMultiAccountSupportedAndUserActive()) {
      // In multiprofile/multiaccount case |user_card_view_| and
      // |logout_button_| have to have the same height.
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
  } else if (user_card_view_) {
    user_card_view_->SetBoundsRect(contents_area);
  } else if (logout_button_) {
    logout_button_->SetBoundsRect(contents_area);
  }
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == logout_button_) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_SIGN_OUT);
    RemoveAddUserMenuOption();
    Shell::GetInstance()->system_tray_delegate()->SignOut();
  } else if (sender == user_card_view_ && !multiprofile_index_ &&
             IsMultiAccountSupportedAndUserActive()) {
    owner_->TransitionDetailedView();
  } else if (sender == user_card_view_ &&
             IsMultiProfileSupportedAndUserActive()) {
    if (!multiprofile_index_) {
      ToggleAddUserMenuOption();
    } else {
      RemoveAddUserMenuOption();
      SwitchUser(multiprofile_index_);
      // Since the user list is about to change the system menu should get
      // closed.
      owner_->system_tray()->CloseSystemBubble();
    }
  } else if (add_menu_option_.get() &&
             sender == add_menu_option_->GetContentsView()) {
    RemoveAddUserMenuOption();
    // Let the user add another account to the session.
    MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
    Shell::GetInstance()->system_tray_delegate()->ShowUserLogin();
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

void UserView::AddLogoutButton(user::LoginStatus login) {
  const base::string16 title =
      user::GetLocalizedSignOutStringForStatus(login, true);
  TrayPopupLabelButton* logout_button =
      new LogoutButton(this, title, for_detailed_view_);
  logout_button->SetAccessibleName(title);
  logout_button_ = logout_button;
  // In public account mode, the logout button border has a custom color.
  if (login == user::LOGGED_IN_PUBLIC) {
    scoped_ptr<TrayPopupLabelButtonBorder> border(
        new TrayPopupLabelButtonBorder());
    border->SetPainter(false,
                       views::Button::STATE_NORMAL,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesNormal));
    border->SetPainter(false,
                       views::Button::STATE_HOVERED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    border->SetPainter(false,
                       views::Button::STATE_PRESSED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    logout_button_->SetBorder(border.PassAs<views::Border>());
  }
  AddChildView(logout_button_);
}

void UserView::AddUserCard(user::LoginStatus login) {
  // Add padding around the panel.
  SetBorder(views::Border::CreateEmptyBorder(kTrayPopupUserCardVerticalPadding,
                                             kTrayPopupPaddingHorizontal,
                                             kTrayPopupUserCardVerticalPadding,
                                             kTrayPopupPaddingHorizontal));

  views::TrayBubbleView* bubble_view =
      owner_->system_tray()->GetSystemBubble()->bubble_view();
  int max_card_width =
      bubble_view->GetMaximumSize().width() -
      (2 * kTrayPopupPaddingHorizontal + kTrayPopupPaddingBetweenItems);
  if (logout_button_)
    max_card_width -= logout_button_->GetPreferredSize().width();
  user_card_view_ =
      new UserCardView(login, max_card_width, multiprofile_index_);
  bool clickable = IsMultiProfileSupportedAndUserActive() ||
                   IsMultiAccountSupportedAndUserActive();
  if (clickable) {
    // To allow the border to start before the icon, reduce the size before and
    // add an inset to the icon to get the spacing.
    if (!multiprofile_index_) {
      SetBorder(views::Border::CreateEmptyBorder(
          kTrayPopupUserCardVerticalPadding,
          kTrayPopupPaddingHorizontal - kTrayUserTileHoverBorderInset,
          kTrayPopupUserCardVerticalPadding,
          kTrayPopupPaddingHorizontal));
      user_card_view_->SetBorder(views::Border::CreateEmptyBorder(
          0, kTrayUserTileHoverBorderInset, 0, 0));
    }
    gfx::Insets insets = gfx::Insets(1, 1, 1, 1);
    views::View* contents_view = user_card_view_;
    ButtonFromView* button = NULL;
    if (!for_detailed_view_) {
      if (multiprofile_index_) {
        // Since the activation border needs to be drawn around the tile, we
        // have to put the tile into another view which fills the menu panel,
        // but keeping the offsets of the content.
        contents_view = new views::View();
        contents_view->SetBorder(views::Border::CreateEmptyBorder(
            kTrayPopupUserCardVerticalPadding,
            kTrayPopupPaddingHorizontal,
            kTrayPopupUserCardVerticalPadding,
            kTrayPopupPaddingHorizontal));
        contents_view->SetLayoutManager(new views::FillLayout());
        SetBorder(views::Border::CreateEmptyBorder(0, 0, 0, 0));
        contents_view->AddChildView(user_card_view_);
        insets = gfx::Insets(1, 1, 1, 3);
      }
      button = new ButtonFromView(contents_view,
                                  this,
                                  !multiprofile_index_,
                                  insets);
      // TODO(skuhne): For accessibility we need to call |SetAccessibleName|
      // with a useful name (string freeze for M37 has passed).
    } else {
      // We want user card for detailed view to have exactly the same look
      // as user card for default view. That's why we wrap it in a button
      // without click listener and special hover behavior.
      button = new ButtonFromView(contents_view, NULL, false, insets);
    }
    // A click on the button should not trigger a focus change.
    button->set_request_focus_on_press(false);
    user_card_view_ = button;
    is_user_card_button_ = true;
  }
  AddChildViewAt(user_card_view_, 0);
  // Card for supervised user can consume more space than currently
  // available. In that case we should increase system bubble's width.
  if (login == user::LOGGED_IN_PUBLIC)
    bubble_view->SetWidth(GetPreferredSize().width());
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
  params.context = this->GetWidget()->GetNativeWindow();
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  add_menu_option_->Init(params);
  add_menu_option_->SetOpacity(0xFF);
  add_menu_option_->GetNativeWindow()->set_owned_by_parent(false);
  SetShadowType(add_menu_option_->GetNativeView(), wm::SHADOW_TYPE_NONE);

  // Position it below our user card.
  gfx::Rect bounds = user_card_view_->GetBoundsInScreen();
  bounds.set_y(bounds.y() + bounds.height());
  add_menu_option_->SetBounds(bounds);

  // Show the content.
  add_menu_option_->SetAlwaysOnTop(true);
  add_menu_option_->Show();

  AddUserView* add_user_view =
      new AddUserView(static_cast<ButtonFromView*>(user_card_view_));

  const SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();

  bool multi_profile_allowed =
      delegate->IsMultiProfileAllowedByPrimaryUserPolicy();
  add_user_disabled_ = (delegate->NumberOfLoggedInUsers() >=
                        delegate->GetMaximumNumberOfLoggedInUsers()) ||
                       !multi_profile_allowed;

  ButtonFromView* button = new ButtonFromView(
      add_user_view,
      add_user_disabled_ ? NULL : this,
      !add_user_disabled_,
      gfx::Insets(1, 1, 1, 1));
  button->set_request_focus_on_press(false);
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  button->ForceBorderVisible(true);
  add_menu_option_->SetContentsView(button);

  if (add_user_disabled_) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    int message_id;
    if (!multi_profile_allowed)
      message_id = IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER;
    else
      message_id = IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER;

    popup_message_.reset(new PopupMessage(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAPTION_CANNOT_ADD_USER),
        bundle.GetLocalizedString(message_id),
        PopupMessage::ICON_WARNING,
        add_user_view->anchor(),
        views::BubbleBorder::TOP_LEFT,
        gfx::Size(parent()->bounds().width() - kPopupMessageOffset, 0),
        2 * kPopupMessageOffset));
  } else {
    // We activate the entry automatically if invoked with focus.
    if (user_card_view_->HasFocus()) {
      button->GetFocusManager()->SetFocusedView(button);
      user_card_view_->GetFocusManager()->SetFocusedView(button);
    }
  }
  // Find the screen area which encloses both elements and sets then a mouse
  // watcher which will close the "menu".
  gfx::Rect area = user_card_view_->GetBoundsInScreen();
  area.set_height(2 * area.height());
  mouse_watcher_.reset(
      new views::MouseWatcher(new UserViewMouseWatcherHost(area), this));
  mouse_watcher_->Start();
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
  focus_manager_ = NULL;
  if (user_card_view_->GetFocusManager())
    user_card_view_->GetFocusManager()->ClearFocus();
  popup_message_.reset();
  mouse_watcher_.reset();
  add_menu_option_.reset();
}

}  // namespace tray
}  // namespace ash
