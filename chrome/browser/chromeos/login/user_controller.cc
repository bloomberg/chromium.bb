// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/login/existing_user_view.h"
#include "chrome/browser/chromeos/login/guest_user_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/username_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/painter.h"
#include "views/screen.h"
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
// case to make border window size close to existing users.
#if defined(CROS_FONTS_USING_BCI)
const int kControlsHeight = 31;
#else
const int kControlsHeight = 28;
#endif

// Widget that notifies window manager about clicking on itself.
// Doesn't send anything if user is selected.
class ClickNotifyingWidget : public views::WidgetGtk {
 public:
  ClickNotifyingWidget(views::WidgetGtk::Type type,
                       UserController* controller)
      : WidgetGtk(type),
        controller_(controller) {
  }

 private:
  gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
    if (!controller_->is_user_selected())
      controller_->SelectUser(controller_->user_index());

    return views::WidgetGtk::OnButtonPress(widget, event);
  }

  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ClickNotifyingWidget);
};

void CloseWindow(views::WidgetGtk* window) {
  if (!window)
    return;
  window->SetWidgetDelegate(NULL);
  window->Close();
}

}  // namespace

using login::kBackgroundColor;
using login::kBorderSize;
using login::kTextColor;
using login::kUserImageSize;

// static
const int UserController::kPadding = 30;

// Max size needed when an entry is not selected.
const int UserController::kUnselectedSize = 100;
const int UserController::kNewUserUnselectedSize = 42;

UserController::UserController(Delegate* delegate, bool is_guest)
    : user_index_(-1),
      is_user_selected_(false),
      is_new_user_(!is_guest),
      is_guest_(is_guest),
      is_owner_(false),
      show_name_tooltip_(false),
      delegate_(delegate),
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
      user_view_(NULL),
      new_user_view_(NULL),
      existing_user_view_(NULL),
      guest_user_view_(NULL),
      label_view_(NULL),
      unselected_label_view_(NULL) {
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      NotificationService::AllSources());
}

UserController::UserController(Delegate* delegate,
                               const UserManager::User& user)
    : user_index_(-1),
      is_user_selected_(false),
      is_new_user_(false),
      is_guest_(false),
      // Empty 'cached_owner()' means that owner hasn't been cached yet, not
      // that owner has an empty email.
      is_owner_(
          !user.email().empty() &&
          UserCrosSettingsProvider::cached_owner() == user.email()),
      show_name_tooltip_(false),
      user_(user),
      delegate_(delegate),
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
      user_view_(NULL),
      new_user_view_(NULL),
      existing_user_view_(NULL),
      guest_user_view_(NULL),
      label_view_(NULL),
      unselected_label_view_(NULL) {
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      NotificationService::AllSources());
}

UserController::~UserController() {
  // Reset the widget delegate of every window to NULL, so the user
  // controller will not get notified about the active window change.
  // See also crosbug.com/7400.
  CloseWindow(controls_window_);
  CloseWindow(image_window_);
  CloseWindow(border_window_);
  CloseWindow(label_window_);
  CloseWindow(unselected_label_window_);
}

void UserController::Init(int index,
                          int total_user_count,
                          bool need_browse_without_signin) {
  int controls_height = 0;
  int controls_width = 0;
  controls_window_ =
      CreateControlsWindow(index, &controls_width, &controls_height,
                           need_browse_without_signin);
  image_window_ = CreateImageWindow(index);
  CreateBorderWindow(index, total_user_count, controls_width, controls_height);
  label_window_ = CreateLabelWindow(index, WM_IPC_WINDOW_LOGIN_LABEL);
  unselected_label_window_ =
      CreateLabelWindow(index, WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL);
}

void UserController::SetPasswordEnabled(bool enable) {
  DCHECK(!is_new_user_);
  existing_user_view_->password_field()->SetEnabled(enable);
  if (enable) {
    user_view_->StopThrobber();
    delegate_->SetStatusAreaEnabled(enable);
  } else {
    delegate_->SetStatusAreaEnabled(enable);
    user_view_->StartThrobber();
  }
}

std::wstring UserController::GetNameTooltip() const {
  if (is_new_user_)
    return l10n_util::GetString(IDS_ADD_USER);
  if (is_guest_)
    return l10n_util::GetString(IDS_GO_INCOGNITO_BUTTON);

  // Tooltip contains user's display name and his email domain to distinguish
  // this user from the other one with the same display name.
  const std::string& email = user_.email();
  size_t at_pos = email.rfind('@');
  if (at_pos == std::string::npos) {
    NOTREACHED();
    return std::wstring();
  }
  size_t domain_start = at_pos + 1;
  std::string domain = email.substr(domain_start,
                                    email.length() - domain_start);
  return UTF8ToWide(base::StringPrintf("%s (%s)",
                                       user_.GetDisplayName().c_str(),
                                       domain.c_str()));
}

void UserController::ClearAndEnablePassword() {
  if (is_new_user_) {
    // TODO(avayvod): This code seems not reachable to me.
    new_user_view_->ClearAndEnablePassword();
  } else {
    existing_user_view_->password_field()->SetText(string16());
    SetPasswordEnabled(true);
    FocusPasswordField();
  }
}

void UserController::ClearAndEnableFields() {
  if (is_new_user_) {
    new_user_view_->ClearAndEnableFields();
  } else if (is_guest_) {
    guest_user_view_->FocusSignInButton();
  } else {
    ClearAndEnablePassword();
  }
}

void UserController::EnableNameTooltip(bool enable) {
  std::wstring tooltip_text;
  if (enable)
    tooltip_text = GetNameTooltip();

  if (user_view_)
    user_view_->SetTooltipText(tooltip_text);
  if (label_view_)
    label_view_->SetTooltipText(tooltip_text);
  if (unselected_label_view_)
    unselected_label_view_->SetTooltipText(tooltip_text);
}

void UserController::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  Login();
}

bool UserController::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == app::VKEY_RETURN) {
    Login();
    return true;
  } else if (keystroke.GetKeyboardCode() == app::VKEY_LEFT) {
    SelectUser(user_index() - 1);
    return true;
  } else if (keystroke.GetKeyboardCode() == app::VKEY_RIGHT) {
    SelectUser(user_index() + 1);
    return true;
  }
  delegate_->ClearErrors();
  return false;
}

void UserController::ContentsChanged(views::Textfield* sender,
                                     const string16& new_contents) {
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
  user_view_->SetImage(user_.image(), user_.image());
}

void UserController::Login() {
  if (is_guest_) {
    delegate_->LoginOffTheRecord();
  } else {
    // Delegate will reenable as necessary.
    SetPasswordEnabled(false);

    delegate_->Login(this, existing_user_view_->password_field()->text());
  }
}

void UserController::IsActiveChanged(bool active) {
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

void UserController::ConfigureLoginWindow(WidgetGtk* window,
                                          int index,
                                          const gfx::Rect& bounds,
                                          chromeos::WmIpcWindowType type,
                                          views::View* contents_view) {
  window->MakeTransparent();
  window->Init(NULL, bounds);
  window->SetContentsView(contents_view);
  window->SetWidgetDelegate(this);

  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      type,
      &params);

  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  window->Show();
}

WidgetGtk* UserController::CreateControlsWindow(
    int index,
    int* width, int* height,
    bool need_browse_without_signin) {
  views::View* control_view;
  if (is_new_user_) {
    new_user_view_ =
        new NewUserView(this, true, need_browse_without_signin);
    new_user_view_->Init();
    control_view = new_user_view_;
  } else if (is_guest_) {
    guest_user_view_ = new GuestUserView(this);
    guest_user_view_->RecreateFields();
    control_view = guest_user_view_;
  } else {
    existing_user_view_ = new ExistingUserView(this);
    existing_user_view_->RecreateFields();
    control_view = existing_user_view_;
  }

  *height = kControlsHeight;
  *width = kUserImageSize;
  if (is_new_user_) {
    DCHECK(new_user_view_);
    gfx::Size size = new_user_view_->GetPreferredSize();
    *width = size.width();
    *height = size.height();
  }

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  ConfigureLoginWindow(window,
                       index,
                       gfx::Rect(*width, *height),
                       WM_IPC_WINDOW_LOGIN_CONTROLS,
                       control_view);
  return window;
}

WidgetGtk* UserController::CreateImageWindow(int index) {
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

  WidgetGtk* window = new ClickNotifyingWidget(WidgetGtk::TYPE_WINDOW, this);
  ConfigureLoginWindow(window,
                       index,
                       gfx::Rect(user_view_->GetPreferredSize()),
                       WM_IPC_WINDOW_LOGIN_IMAGE,
                       user_view_);

  return window;
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
    height += 3 * kBorderSize + kUserImageSize;
  }

  border_window_ = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  border_window_->MakeTransparent();
  border_window_->Init(NULL, gfx::Rect(0, 0, width, height));
  if (!is_new_user_) {
    views::Painter* painter = CreateWizardPainter(
        &BorderDefinition::kUserBorder);
    border_window_->GetRootView()->set_background(
        views::Background::CreateBackgroundPainter(true, painter));
  }
  UpdateUserCount(index, total_user_count);

  GdkWindow* gdk_window = border_window_->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  border_window_->Show();
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

WidgetGtk* UserController::CreateLabelWindow(int index,
                                             WmIpcWindowType type) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      rb.GetFont(ResourceBundle::MediumBoldFont) :
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
          kUnselectedUsernameFontDelta, gfx::Font::BOLD);
  std::wstring text;
  if (is_guest_) {
    text = l10n_util::GetString(IDS_GUEST);
  } else if (is_new_user_) {
    // Add user should have label only in activated state.
    // When new user is the only, label is not needed.
    if (type == WM_IPC_WINDOW_LOGIN_LABEL && index != 0)
      text = l10n_util::GetString(IDS_ADD_USER);
  } else {
    text = UTF8ToWide(user_.GetDisplayName());
  }

  views::Label *label = is_new_user_ ?
      new views::Label(text) : new UsernameView(text);

  label->SetColor(kTextColor);
  label->SetFont(font);

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
  WidgetGtk* window = new ClickNotifyingWidget(WidgetGtk::TYPE_WINDOW, this);
  ConfigureLoginWindow(window,
                       index,
                       gfx::Rect(0, 0, width, height),
                       type,
                       label);
  return window;
}

gfx::Rect UserController::GetScreenBounds() const {
  if (is_new_user_)
    return new_user_view_->GetUsernameBounds();
  else
    return existing_user_view_->password_field()->GetScreenBounds();
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

void UserController::NavigateAway() {
  SelectUser(user_index() - 1);
}

void UserController::OnRemoveUser() {
  delegate_->RemoveUser(this);
}

void UserController::SelectUser(int index) {
  delegate_->SelectUser(index);
}

void UserController::FocusPasswordField() {
  existing_user_view_->FocusPasswordField();
}

}  // namespace chromeos
