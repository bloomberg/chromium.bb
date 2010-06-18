// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "views/background.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/grid_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

using views::ColumnSet;
using views::GridLayout;
using views::WidgetGtk;

namespace chromeos {

namespace {

// Gap between the border around the image/buttons and user name.
const int kUserNameGap = 4;

// Approximate height of controls window, this constant is used in new user
// case to make border window size close to exsisting users.
const int kControlsHeight = 30;

}  // namespace

using login::kBackgroundColor;
using login::kBorderSize;
using login::kTextColor;
using login::kUserImageSize;

// static
const int UserController::kPadding = 20;

// Max size needed when an entry is not selected.
const int UserController::kUnselectedSize = 100;

UserController::UserController(Delegate* delegate)
    : is_guest_(true),
      delegate_(delegate),
      password_field_(NULL),
      submit_button_(NULL),
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
      user_view_(NULL),
      new_user_view_(NULL) {
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      NotificationService::AllSources());
}

UserController::UserController(Delegate* delegate,
                               const UserManager::User& user)
    : is_guest_(false),
      user_(user),
      delegate_(delegate),
      password_field_(NULL),
      submit_button_(NULL),
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
      user_view_(NULL),
      new_user_view_(NULL) {
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      NotificationService::AllSources());
}

UserController::~UserController() {
  controls_window_->Close();
  image_window_->Close();
  user_view_ = NULL;
  new_user_view_ = NULL;
  border_window_->Close();
  label_window_->Close();
  unselected_label_window_->Close();
}

void UserController::Init(int index, int total_user_count) {
  int controls_height = 0;
  controls_window_ = CreateControlsWindow(index, &controls_height);
  image_window_ = CreateImageWindow(index);
  border_window_ = CreateBorderWindow(index, total_user_count,
                                      controls_height);
  label_window_ = CreateLabelWindow(index, WM_IPC_WINDOW_LOGIN_LABEL);
  unselected_label_window_ =
      CreateLabelWindow(index, WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL);
}

void UserController::SetPasswordEnabled(bool enable) {
  DCHECK(!is_guest_);
  password_field_->SetEnabled(enable);
  submit_button_->SetEnabled(enable);
  enable ? user_view_->StopThrobber() : user_view_->StartThrobber();
}

void UserController::ClearAndEnablePassword() {
  if (is_guest_) {
    new_user_view_->ClearAndEnablePassword();
  } else {
    password_field_->SetText(string16());
    SetPasswordEnabled(true);
  }
}

void UserController::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  Login();
}

bool UserController::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    Login();
    return true;
  }
  delegate_->ClearErrors();
  return false;
}

void UserController::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type != NotificationType::LOGIN_USER_IMAGE_CHANGED ||
      !user_view_)
    return;

  UserManager::User* user = Details<UserManager::User>(details).ptr();
  if (user_.email() != user->email())
    return;

  user_.set_image(user->image());
  user_view_->SetImage(user_.image());
}

void UserController::Login() {
  // Delegate will reenable as necessary.
  SetPasswordEnabled(false);

  delegate_->Login(this, password_field_->text());
}

void UserController::IsActiveChanged(bool active) {
  if (active)
    delegate_->OnUserSelected(this);
  else
    delegate_->ClearErrors();
}

WidgetGtk* UserController::CreateControlsWindow(int index, int* height) {
  views::View* control_view;
  if (is_guest_) {
    new_user_view_ = new NewUserView(this, false);
    new_user_view_->Init();
    control_view = new_user_view_;
  } else {
    password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
    password_field_->set_text_to_display_when_empty(
        l10n_util::GetStringUTF16(IDS_LOGIN_EMPTY_PASSWORD_TEXT));
    password_field_->SetController(this);
    submit_button_ = new views::NativeButton(
        this, l10n_util::GetString(IDS_LOGIN_BUTTON));
    control_view = new views::View();
    GridLayout* layout = new GridLayout(control_view);
    control_view->SetLayoutManager(layout);
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kBorderSize);
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                          GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    layout->AddView(password_field_);
    layout->AddView(submit_button_);
  }

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->MakeTransparent();
  window->Init(NULL, gfx::Rect());
  window->SetContentsView(control_view);
  window->SetWidgetDelegate(this);
  *height = is_guest_ ? kUserImageSize + kControlsHeight
                      : control_view->GetPreferredSize().height();
  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      WM_IPC_WINDOW_LOGIN_CONTROLS,
      &params);
  window->SetBounds(gfx::Rect(0, 0, kUserImageSize, *height));
  window->Show();
  return window;
}

WidgetGtk* UserController::CreateImageWindow(int index) {
  user_view_ = new UserView();

  if (!is_guest_) {
    user_view_->SetImage(user_.image());
    user_view_->SetTooltipText(UTF8ToWide(user_.email()));
  } else {
    user_view_->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_LOGIN_OTHER_USER));
  }

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, gfx::Rect(user_view_->GetPreferredSize()));
  window->SetContentsView(user_view_);

  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      WM_IPC_WINDOW_LOGIN_IMAGE,
      &params);
  window->Show();
  return window;
}

WidgetGtk* UserController::CreateBorderWindow(int index,
                                              int total_user_count,
                                              int controls_height) {
  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, gfx::Rect());
  window->GetRootView()->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  std::vector<int> params;
  params.push_back(index);
  params.push_back(total_user_count);
  params.push_back(kUnselectedSize);
  params.push_back(kPadding);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      WM_IPC_WINDOW_LOGIN_BORDER,
      &params);

  // Guest login controls window is much higher than exsisting user's controls
  // window so window manager will place the control instead of image window.
  int height = kBorderSize * 2 + controls_height;
  height += is_guest_ ? 0 : kBorderSize + kUserImageSize;
  window->SetBounds(gfx::Rect(0, 0, kUserImageSize + kBorderSize * 2, height));
  window->Show();
  return window;
}

WidgetGtk* UserController::CreateLabelWindow(int index,
                                             WmIpcWindowType type) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      rb.GetFont(ResourceBundle::LargeFont).DeriveFont(0, gfx::Font::BOLD) :
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  int width = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      kUserImageSize : kUnselectedSize;
  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->MakeTransparent();
  window->Init(NULL, gfx::Rect());
  std::wstring text = is_guest_ ? l10n_util::GetString(IDS_GUEST) :
      UTF8ToWide(user_.GetDisplayName());
  views::Label* label = new views::Label(text);
  label->SetColor(kTextColor);
  label->SetFont(font);
  window->SetContentsView(label);
  int height = label->GetPreferredSize().height();
  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(window->GetNativeView(), type, &params);
  window->SetBounds(gfx::Rect(0, 0, width, height));
  window->Show();
  return window;
}

gfx::Rect UserController::GetScreenBounds() const {
  if (is_guest_) {
    return new_user_view_->GetPasswordBounds();
  } else {
    gfx::Rect screen_bounds(password_field_->bounds());
    gfx::Point origin(screen_bounds.origin());
    views::View::ConvertPointToScreen(password_field_->GetParent(), &origin);
    screen_bounds.set_origin(origin);
    return screen_bounds;
  }
}

void UserController::OnLogin(const std::string& username,
                             const std::string& password) {
  user_.set_email(username);
  delegate_->Login(this, UTF8ToUTF16(password));
}

void UserController::OnCreateAccount() {
  delegate_->ActivateWizard(WizardController::kAccountScreenName);
}

void UserController::OnLoginOffTheRecord() {
  delegate_->LoginOffTheRecord();
}

void UserController::ClearErrors() {
  delegate_->ClearErrors();
}

}  // namespace chromeos
