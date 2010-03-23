// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/controls/image_view.h"
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

// Gap between edge and image view, and image view and controls.
const int kBorderSize = 4;

// Gap between the border around the image/buttons and user name.
const int kUserNameGap = 4;

// Background color.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color.
const SkColor kTextColor = SK_ColorWHITE;

}  // namespace

// static
const int UserController::kSize = 260;

// static
const int UserController::kPadding = 20;

// Max size needed when an entry is not selected.
const int UserController::kUnselectedSize = 100;

UserController::UserController()
    : is_guest_(true),
      delegate_(NULL),
      password_field_(NULL),
      submit_button_(NULL),
      controls_window_(NULL),
      image_window_(NULL),
      border_window_(NULL),
      label_window_(NULL),
      unselected_label_window_(NULL) {
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
      unselected_label_window_(NULL) {
}

UserController::~UserController() {
  controls_window_->Close();
  image_window_->Close();
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
  label_window_ = CreateLabelWindow(index, WmIpc::WINDOW_TYPE_LOGIN_LABEL);
  unselected_label_window_ =
      CreateLabelWindow(index, WmIpc::WINDOW_TYPE_LOGIN_UNSELECTED_LABEL);
}

void UserController::SetPasswordEnabled(bool enable) {
  password_field_->SetEnabled(enable);
  submit_button_->SetEnabled(enable);
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
  return false;
}

void UserController::Login() {
  // Delegate will reenable as necessary.
  SetPasswordEnabled(false);

  delegate_->Login(this, password_field_->text());
}

WidgetGtk* UserController::CreateControlsWindow(int index, int* height) {
  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  password_field_->SetController(this);
  submit_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_LOGIN_BUTTON));
  views::View* control_view = new views::View();
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

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->MakeTransparent();
  window->Init(NULL, gfx::Rect());
  window->SetContentsView(control_view);
  gfx::Size pref = control_view->GetPreferredSize();
  *height = pref.height();
  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      WmIpc::WINDOW_TYPE_LOGIN_CONTROLS,
      &params);
  window->SetBounds(gfx::Rect(0, 0, kSize, pref.height()));
  window->Show();
  return window;
}

WidgetGtk* UserController::CreateImageWindow(int index) {
  views::ImageView* image_view = new views::ImageView();
  image_view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  if (!is_guest_) {
    image_view->SetImage(user_.image());
  } else {
    image_view->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
                             IDR_LOGIN_OTHER_USER));
  }
  image_view->SetImageSize(gfx::Size(kSize, kSize));
  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, gfx::Rect());
  window->SetContentsView(image_view);
  std::vector<int> params;
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      WmIpc::WINDOW_TYPE_LOGIN_IMAGE,
      &params);
  window->SetBounds(gfx::Rect(0, 0, kSize, kSize));
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
      WmIpc::WINDOW_TYPE_LOGIN_BORDER,
      &params);

  window->SetBounds(gfx::Rect(0, 0, kSize + kBorderSize * 2,
                              kSize + kBorderSize * 2 +
                              kBorderSize + controls_height));
  window->Show();
  return window;
}

WidgetGtk* UserController::CreateLabelWindow(int index,
                                             WmIpc::WindowType type) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = (type == WmIpc::WINDOW_TYPE_LOGIN_LABEL) ?
      rb.GetFont(ResourceBundle::LargeFont).DeriveFont(0, gfx::Font::BOLD) :
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  int width = (type == WmIpc::WINDOW_TYPE_LOGIN_LABEL) ?
      kSize : kUnselectedSize;
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

}  // namespace chromeos
