// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/login/ui.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/login/login.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/shell/public/cpp/connector.h"
#include "ui/views/background.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"

namespace mash {
namespace login {

// static
bool UI::is_showing_ = false;

// static
void UI::Show(mojo::Connector* connector, LoginController* login_controller) {
  // It's possible multiple clients may have a connection to the Login service,
  // so make sure that only one login UI can be shown at a time.
  if (is_showing_)
    return;

  UI* ui = new UI(login_controller, connector);

  // TODO(beng): If this is only done once, it should be done in
  // LoginController::Initialize(). However, for as yet unknown reasons it needs
  // to be done the first time after UI(). Figure this out. Also, I'm not
  // certain the window manager is being killed when this UI is closed.
  if (!views::WindowManagerConnection::Exists())
    views::WindowManagerConnection::Create(connector);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = ui;

  std::map<std::string, std::vector<uint8_t>> properties;
  properties[mash::wm::mojom::kWindowContainer_Property] =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
          static_cast<int32_t>(mash::wm::mojom::Container::LOGIN_WINDOWS));
  mus::Window* window =
      views::WindowManagerConnection::Get()->NewWindow(properties);
  params.native_widget = new views::NativeWidgetMus(
      widget, connector, window, mus::mojom::SurfaceType::DEFAULT);
  widget->Init(params);
  widget->Show();

  is_showing_ = true;
}

UI::UI(LoginController* login_controller, mojo::Connector* connector)
    : login_controller_(login_controller),
      connector_(connector),
      user_id_1_("00000000-0000-4000-8000-000000000000"),
      user_id_2_("00000000-0000-4000-8000-000000000001"),
      login_button_1_(
          new views::LabelButton(this, base::ASCIIToUTF16("Timothy"))),
      login_button_2_(
          new views::LabelButton(this, base::ASCIIToUTF16("Jimothy"))) {
  connector_->ConnectToInterface("mojo:mus", &user_access_manager_);
  user_access_manager_->SetActiveUser(login_controller->login_user_id());
  StartWindowManager();

  set_background(views::Background::CreateSolidBackground(SK_ColorRED));
  login_button_1_->SetStyle(views::Button::STYLE_BUTTON);
  login_button_2_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(login_button_1_);
  AddChildView(login_button_2_);
}

UI::~UI() {
  // Prevent the window manager from restarting during graceful shutdown.
  window_manager_connection_->SetConnectionLostClosure(base::Closure());
  is_showing_ = false;
}

views::View* UI::GetContentsView() { return this; }

base::string16 UI::GetWindowTitle() const {
  // TODO(beng): use resources.
  return base::ASCIIToUTF16("Login");
}

void UI::DeleteDelegate() {
  delete this;
}

void UI::Layout() {
  gfx::Rect button_box = GetLocalBounds();
  button_box.Inset(10, 10);

  gfx::Size ps1 = login_button_1_->GetPreferredSize();
  gfx::Size ps2 = login_button_2_->GetPreferredSize();

  DCHECK(ps1.height() == ps2.height());

  // The 10 is inter-button spacing.
  button_box.set_x((button_box.width() - ps1.width() - ps2.width() - 10) / 2);
  button_box.set_y((button_box.height() - ps1.height()) / 2);

  login_button_1_->SetBounds(button_box.x(), button_box.y(), ps1.width(),
                              ps1.height());
  login_button_2_->SetBounds(login_button_1_->bounds().right() + 10,
                              button_box.y(), ps2.width(), ps2.height());
}

void UI::ButtonPressed(views::Button* sender, const ui::Event& event) {
  // Login...
  if (sender == login_button_1_) {
    user_access_manager_->SetActiveUser(user_id_1_);
    login_controller_->init()->StartService("mojo:mash_shell", user_id_1_);
  } else if (sender == login_button_2_) {
    user_access_manager_->SetActiveUser(user_id_2_);
    login_controller_->init()->StartService("mojo:mash_shell", user_id_2_);
  } else {
    NOTREACHED();
  }
  GetWidget()->Close();
}

void UI::StartWindowManager() {
  window_manager_connection_ = connector_->Connect("mojo:desktop_wm");
  window_manager_connection_->SetConnectionLostClosure(
      base::Bind(&UI::StartWindowManager, base::Unretained(this)));
}

}  // namespace login
}  // namespace mash
