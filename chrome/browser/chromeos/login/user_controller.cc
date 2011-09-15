// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include <algorithm>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/existing_user_view.h"
#include "chrome/browser/chromeos/login/guest_user_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/username_view.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/background.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/focus/focus_manager.h"
#include "views/painter.h"
#include "views/widget/widget.h"

using views::Widget;

namespace chromeos {

namespace {

// Gap between the border around the image/buttons and user name.
const int kUserNameGap = 4;

// Approximate height of controls window, this constant is used in new user
// case to make border window size close to existing users.
#if defined(CROS_FONTS_USING_BCI)
const int kControlsHeight = 31;
#else
const int kControlsHeight = 28;
#endif

// Vertical interval between the image and the textfield.
const int kVerticalIntervalSize = 10;

void CloseWindow(views::Widget* window) {
  if (window)
    window->CloseNow();
}

}  // namespace

// WidgetDelegate implementation for the Widget used for the controls.
class UserController::ControlsWidgetDelegate : public views::WidgetDelegate {
 public:
  ControlsWidgetDelegate(UserController* controller,
                         views::View* view)
      : controller_(controller),
        view_(view) {
  }

  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return view_;
  }

  virtual views::Widget* GetWidget() OVERRIDE {
    return view_->GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return view_->GetWidget();
  }

 private:
  UserController* controller_;

  // View to give focus to on show.
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ControlsWidgetDelegate);
};

using login::kBorderSize;
using login::kUserImageSize;

// static
const int UserController::kPadding = 30;

// Max size needed when an entry is not selected.
const int UserController::kUnselectedSize = 100;
const int UserController::kNewUserUnselectedSize = 42;

////////////////////////////////////////////////////////////////////////////////
// UserController, public:

UserController::UserController(Delegate* delegate, bool is_guest)
    : user_index_(-1),
      is_user_selected_(false),
      is_new_user_(!is_guest),
      is_guest_(is_guest),
      is_owner_(false),
      show_name_tooltip_(false),
      delegate_(delegate),
      controls_widget_(NULL),
      image_widget_(NULL),
      border_window_(NULL),
      label_widget_(NULL),
      unselected_label_widget_(NULL),
      user_view_(NULL),
      label_view_(NULL),
      unselected_label_view_(NULL),
      user_input_(NULL),
      throbber_host_(NULL) {
}

UserController::UserController(Delegate* delegate,
                               const UserManager::User& user)
    : user_index_(-1),
      is_user_selected_(false),
      is_new_user_(false),
      is_guest_(false),
      // Empty 'cached_owner()' means that owner hasn't been cached yet, not
      // that owner has an empty email.
      is_owner_(user.email() == UserCrosSettingsProvider::cached_owner()),
      show_name_tooltip_(false),
      user_(user),
      delegate_(delegate),
      controls_widget_(NULL),
      image_widget_(NULL),
      border_window_(NULL),
      label_widget_(NULL),
      unselected_label_widget_(NULL),
      user_view_(NULL),
      label_view_(NULL),
      unselected_label_view_(NULL),
      user_input_(NULL),
      throbber_host_(NULL) {
  DCHECK(!user.email().empty());
}

UserController::~UserController() {
  // Reset the widget delegate of every window to NULL, so the user
  // controller will not get notified about the active window change.
  // See also crosbug.com/7400.
  CloseWindow(controls_widget_);
  CloseWindow(image_widget_);
  CloseWindow(border_window_);
  CloseWindow(label_widget_);
  CloseWindow(unselected_label_widget_);
}

void UserController::Init(int index,
                          int total_user_count,
                          bool need_browse_without_signin) {
  int controls_height = 0;
  int controls_width = 0;
  SetupControlsWidget(index, &controls_width, &controls_height,
                      need_browse_without_signin);
  image_widget_ = CreateImageWidget(index);
  CreateBorderWindow(index, total_user_count, controls_width, controls_height);
  label_widget_ = CreateLabelWidget(index, WM_IPC_WINDOW_LOGIN_LABEL);
  unselected_label_widget_ =
      CreateLabelWidget(index, WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL);
}

void UserController::ClearAndEnableFields() {
  user_input_->EnableInputControls(true);
  user_input_->ClearAndFocusControls();
  StopThrobber();
}

void UserController::ClearAndEnablePassword() {
  // Somehow focus manager thinks that textfield is still focused but the
  // textfield doesn't know that. So we clear focus for focus manager so it
  // sets focus on the textfield again.
  // TODO(avayvod): Fix the actual issue.
  views::FocusManager* focus_manager = controls_widget_->GetFocusManager();
  if (focus_manager)
    focus_manager->ClearFocus();
  user_input_->EnableInputControls(true);
  user_input_->ClearAndFocusPassword();
  StopThrobber();
}

void UserController::EnableNameTooltip(bool enable) {
  name_tooltip_enabled_ = enable;
  string16 tooltip_text;
  if (enable)
    tooltip_text = GetNameTooltip();

  if (user_view_)
    user_view_->SetTooltipText(tooltip_text);
  if (label_view_)
    label_view_->SetTooltipText(tooltip_text);
  if (unselected_label_view_)
    unselected_label_view_->SetTooltipText(tooltip_text);
}

gfx::Rect UserController::GetMainInputScreenBounds() const {
  return user_input_->GetMainInputScreenBounds();
}

void UserController::OnUserImageChanged(UserManager::User* user) {
  if (user_.email() != user->email())
    return;
  user_.SetImage(user->image(), user->default_image_index());
  // Controller might exist without windows,
  // i.e. if user pod doesn't fit on the screen.
  if (user_view_)
    user_view_->SetImage(user_.image(), user_.image());
}

void UserController::SelectUserRelative(int shift) {
  delegate_->SelectUser(user_index() + shift);
}

void UserController::StartThrobber() {
  throbber_host_->StartThrobber();
}

void UserController::StopThrobber() {
  throbber_host_->StopThrobber();
}

void UserController::UpdateUserCount(int index, int total_user_count) {
  user_index_ = index;
  std::vector<int> params;
  params.push_back(index);
  params.push_back(total_user_count);
  params.push_back(is_new_user_ ? kNewUserUnselectedSize : kUnselectedSize);
  params.push_back(kPadding);
  WmIpc::instance()->SetWindowType(
      border_window_->GetNativeView(),
      WM_IPC_WINDOW_LOGIN_BORDER,
      &params);
}

std::string UserController::GetAccessibleUserLabel() {
  if (is_new_user_)
    return l10n_util::GetStringUTF8(IDS_ADD_USER);
  if (is_guest_)
    return l10n_util::GetStringUTF8(IDS_GUEST);
  return user_.email();
}

////////////////////////////////////////////////////////////////////////////////
// UserController, WidgetDelegate implementation:
//
views::Widget* UserController::GetWidget() {
  return NULL;
}

const views::Widget* UserController::GetWidget() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// UserController, NewUserView::Delegate implementation:
//
void UserController::OnLogin(const std::string& username,
                             const std::string& password) {
  if (is_new_user_)
    user_.set_email(username);

  user_input_->EnableInputControls(false);
  StartThrobber();

  delegate_->Login(this, UTF8ToUTF16(password));
}

void UserController::OnCreateAccount() {
  user_input_->EnableInputControls(false);
  StartThrobber();

  delegate_->CreateAccount();
}

void UserController::OnStartEnterpriseEnrollment() {
  delegate_->StartEnterpriseEnrollment();
}

void UserController::OnLoginAsGuest() {
  user_input_->EnableInputControls(false);
  StartThrobber();

  delegate_->LoginAsGuest();
}

void UserController::ClearErrors() {
  delegate_->ClearErrors();
}

void UserController::NavigateAway() {
  SelectUserRelative(-1);
}

////////////////////////////////////////////////////////////////////////////////
// UserController, UserView::Delegate implementation:
//
void UserController::OnLocaleChanged() {
  // Update text tooltips on guest and new user pods.
  if (is_guest_ || is_new_user_) {
    if (name_tooltip_enabled_)
      EnableNameTooltip(name_tooltip_enabled_);
  }
  label_view_->SetFont(GetLabelFont());
  unselected_label_view_->SetFont(GetUnselectedLabelFont());
}

void UserController::OnRemoveUser() {
  delegate_->RemoveUser(this);
}

bool UserController::IsUserSelected() const {
  return is_user_selected_;
}

////////////////////////////////////////////////////////////////////////////////
// UserController, views::Widget::Observer implementation:
//
void UserController::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  is_user_selected_ = active;
  if (active) {
    delegate_->OnUserSelected(this);
    user_view_->SetRemoveButtonVisible(
        !is_new_user_ && !is_guest_ && !is_owner_);
  } else {
    user_view_->SetRemoveButtonVisible(false);
    delegate_->ClearErrors();
  }
}

////////////////////////////////////////////////////////////////////////////////
// UserController, private:
//
void UserController::ConfigureAndShow(Widget* widget,
                                      int index,
                                      chromeos::WmIpcWindowType type,
                                      views::View* contents_view) {
  widget->SetContentsView(contents_view);

  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      widget->GetNativeView(),
      type,
      &params);

  widget->Show();
}

void UserController::SetupControlsWidget(
    int index,
    int* width, int* height,
    bool need_browse_without_signin) {
  views::View* control_view;
  if (is_new_user_) {
    NewUserView* new_user_view =
        new NewUserView(this, true, need_browse_without_signin);
    new_user_view->Init();
    control_view = new_user_view;
    user_input_ = new_user_view;
    throbber_host_ = new_user_view;
  } else if (is_guest_) {
    GuestUserView* guest_user_view = new GuestUserView(this);
    guest_user_view->RecreateFields();
    control_view = guest_user_view;
    user_input_ = guest_user_view;
    throbber_host_ = guest_user_view;
  } else {
    ExistingUserView* existing_user_view = new ExistingUserView(this);
    existing_user_view->RecreateFields();
    control_view = existing_user_view;
    user_input_ = existing_user_view;
    throbber_host_ = existing_user_view;
  }

  *height = kControlsHeight;
  *width = kUserImageSize;
  if (is_new_user_) {
    gfx::Size size = control_view->GetPreferredSize();
    *width = size.width();
    *height = size.height();
  }

  controls_widget_delegate_.reset(
      new ControlsWidgetDelegate(this, control_view));
  controls_widget_ = CreateControlsWidget(controls_widget_delegate_.get(),
                                          gfx::Rect(*width, *height));
  controls_widget_->AddObserver(this);
  ConfigureAndShow(controls_widget_, index, WM_IPC_WINDOW_LOGIN_CONTROLS,
                   control_view);
}

Widget* UserController::CreateImageWidget(int index) {
  user_view_ = new UserView(this, true, !is_new_user_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (is_guest_) {
    SkBitmap* image = rb.GetBitmapNamed(IDR_LOGIN_GUEST);
    user_view_->SetImage(*image, *image);
  } else if (is_new_user_) {
    SkBitmap* image = rb.GetBitmapNamed(IDR_LOGIN_ADD_USER);
    SkBitmap* image_hover = rb.GetBitmapNamed(IDR_LOGIN_ADD_USER_HOVER);
    user_view_->SetImage(*image, *image_hover);
  } else {
    user_view_->SetImage(user_.image(), user_.image());
  }

  Widget* widget =
      CreateClickNotifyingWidget(this,
                                 gfx::Rect(user_view_->GetPreferredSize()));
  widget->AddObserver(this);
  ConfigureAndShow(widget, index, WM_IPC_WINDOW_LOGIN_IMAGE, user_view_);

  return widget;
}

void UserController::CreateBorderWindow(int index,
                                        int total_user_count,
                                        int controls_width,
                                        int controls_height) {
  // New user login controls window is much higher than existing user's controls
  // window so window manager will place the control instead of image window.
  // New user will have 0 size border.
  int width = controls_width;
  int height = controls_height;
  if (!is_new_user_) {
    width += kBorderSize * 2;
    height += 2 * kBorderSize + kUserImageSize + kVerticalIntervalSize;
  }

  border_window_ = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.bounds = gfx::Rect(0, 0, width, height);
  border_window_->Init(params);
  if (!is_new_user_) {
    views::View* background_view = new views::View();
    views::Painter* painter = CreateWizardPainter(
        &BorderDefinition::kUserBorder);
    background_view->set_background(
        views::Background::CreateBackgroundPainter(true, painter));
    border_window_->SetContentsView(background_view);
  }
  UpdateUserCount(index, total_user_count);

  GdkWindow* gdk_window = border_window_->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  border_window_->Show();
}

Widget* UserController::CreateLabelWidget(int index, WmIpcWindowType type) {
  std::wstring text;
  if (is_guest_) {
    text = std::wstring();
  } else if (is_new_user_) {
    // Add user should have label only in activated state.
    // When new user is the only, label is not needed.
    if (type == WM_IPC_WINDOW_LOGIN_LABEL && index != 0)
      text = UTF16ToWide(l10n_util::GetStringUTF16(IDS_ADD_USER));
  } else {
    text = UTF8ToWide(user_.GetDisplayName());
  }

  views::Label* label = NULL;

  if (is_new_user_) {
    label = new views::Label(text);
  } else if (type == WM_IPC_WINDOW_LOGIN_LABEL) {
    label = UsernameView::CreateShapedUsernameView(text, false);
  } else {
    DCHECK(type == WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL);
    // TODO(altimofeev): switch to the rounded username view.
    label = UsernameView::CreateShapedUsernameView(text, true);
  }

  const gfx::Font& font = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      GetLabelFont() : GetUnselectedLabelFont();
  label->SetFont(font);
  label->SetColor(login::kTextColor);

  if (type == WM_IPC_WINDOW_LOGIN_LABEL)
    label_view_ = label;
  else
    unselected_label_view_ = label;

  int width = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      kUserImageSize : kUnselectedSize;
  if (is_new_user_) {
    // Make label as small as possible to don't show tooltip.
    width = 0;
  }
  int height = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      login::kSelectedLabelHeight : login::kUnselectedLabelHeight;
  Widget* widget = CreateClickNotifyingWidget(this,
                                              gfx::Rect(0, 0, width, height));
  ConfigureAndShow(widget, index, type, label);
  return widget;
}

gfx::Font UserController::GetLabelFont() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetFont(ResourceBundle::MediumBoldFont).DeriveFont(
      kSelectedUsernameFontDelta);
}

gfx::Font UserController::GetUnselectedLabelFont() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
      kUnselectedUsernameFontDelta, gfx::Font::BOLD);
}

string16 UserController::GetNameTooltip() const {
  if (is_new_user_)
    return l10n_util::GetStringUTF16(IDS_ADD_USER);
  else if (is_guest_)
    return l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON);
  else
    return UTF8ToUTF16(user_.GetNameTooltip());
}

}  // namespace chromeos
