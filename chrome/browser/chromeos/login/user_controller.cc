// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include <algorithm>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/login/existing_user_view.h"
#include "chrome/browser/chromeos/login/guest_user_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/username_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/layout/grid_layout.h"
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

// Vertical interval between the image and the textfield.
const int kVerticalIntervalSize = 10;

// A window for controls that sets focus to the view when
// it first got focus.
class ControlsWindow : public WidgetGtk {
 public:
  explicit ControlsWindow(views::View* initial_focus_view)
      : WidgetGtk(WidgetGtk::TYPE_WINDOW),
        initial_focus_view_(initial_focus_view) {
  }

 private:
  // WidgetGtk overrrides:
  virtual void SetInitialFocus() {
    if (initial_focus_view_)
      initial_focus_view_->RequestFocus();
  }

  views::View* initial_focus_view_;

  DISALLOW_COPY_AND_ASSIGN(ControlsWindow);
};

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
    if (!controller_->IsUserSelected())
      controller_->SelectUserRelative(0);

    return views::WidgetGtk::OnButtonPress(widget, event);
  }

  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ClickNotifyingWidget);
};

void CloseWindow(views::WidgetGtk* window) {
  if (!window)
    return;
  window->set_widget_delegate(NULL);
  window->Close();
}

}  // namespace

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
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
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
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL),
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

void UserController::ClearAndEnableFields() {
  user_input_->ClearAndFocusControls();
  user_input_->EnableInputControls(true);
  StopThrobber();
}

void UserController::ClearAndEnablePassword() {
  user_input_->ClearAndFocusPassword();
  user_input_->EnableInputControls(true);
  StopThrobber();
}

void UserController::EnableNameTooltip(bool enable) {
  name_tooltip_enabled_ = enable;
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

gfx::Rect UserController::GetMainInputScreenBounds() const {
  return user_input_->GetMainInputScreenBounds();
}

void UserController::OnUserImageChanged(UserManager::User* user) {
  if (user_.email() != user->email())
    return;
  user_.set_image(user->image());
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

////////////////////////////////////////////////////////////////////////////////
// UserController, WidgetDelegate implementation:
//
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
}

void UserController::OnRemoveUser() {
  delegate_->RemoveUser(this);
}

////////////////////////////////////////////////////////////////////////////////
// UserController, private:
//
void UserController::ConfigureLoginWindow(WidgetGtk* window,
                                          int index,
                                          const gfx::Rect& bounds,
                                          chromeos::WmIpcWindowType type,
                                          views::View* contents_view) {
  window->MakeTransparent();
  window->Init(NULL, bounds);
  window->SetContentsView(contents_view);
  window->set_widget_delegate(this);

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

  WidgetGtk* window = new ControlsWindow(control_view);
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
    height += 2 * kBorderSize + kUserImageSize + kVerticalIntervalSize;
  }

  border_window_ = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  border_window_->MakeTransparent();
  border_window_->Init(NULL, gfx::Rect(0, 0, width, height));
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

WidgetGtk* UserController::CreateLabelWindow(int index,
                                             WmIpcWindowType type) {
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

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = (type == WM_IPC_WINDOW_LOGIN_LABEL) ?
      rb.GetFont(ResourceBundle::MediumBoldFont).DeriveFont(
          kSelectedUsernameFontDelta) :
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
          kUnselectedUsernameFontDelta, gfx::Font::BOLD);
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
  WidgetGtk* window = new ClickNotifyingWidget(WidgetGtk::TYPE_WINDOW, this);
  ConfigureLoginWindow(window,
                       index,
                       gfx::Rect(0, 0, width, height),
                       type,
                       label);
  return window;
}

std::wstring UserController::GetNameTooltip() const {
  if (is_new_user_)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_ADD_USER));
  else if (is_guest_)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON));
  else
    return UTF8ToWide(user_.GetNameTooltip());
}

}  // namespace chromeos
