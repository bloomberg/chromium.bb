// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/login/login.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/cpp/connector.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace login {
namespace {

class LoginView : public views::WidgetDelegateView,
                  public views::ButtonListener {
 public:
  explicit LoginView(Login* login)
      : login_(login),
        login_button_1_(
            new views::LabelButton(this, base::ASCIIToUTF16("Timothy"))),
        login_button_2_(
            new views::LabelButton(this, base::ASCIIToUTF16("Jimothy"))) {
    set_background(views::Background::CreateSolidBackground(SK_ColorRED));
    login_button_1_->SetStyle(views::Button::STYLE_BUTTON);
    login_button_2_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(login_button_1_);
    AddChildView(login_button_2_);
  }
  ~LoginView() override {}

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Login");
  }

  // Overridden from views::View:
  void Layout() override {
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

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    // Login...
    mojo::Connector::ConnectParams params("mojo:mash_shell");
    if (sender == login_button_1_) {
      login_->login()->LoginAs(3);
    } else if (sender == login_button_2_) {
      login_->login()->LoginAs(4);
    } else {
      NOTREACHED();
    }
    base::MessageLoop::current()->QuitWhenIdle();
  }

  Login* login_;
  views::LabelButton* login_button_1_;
  views::LabelButton* login_button_2_;

  DISALLOW_COPY_AND_ASSIGN(LoginView);
};

}  // namespace

Login::Login() {}
Login::~Login() {}

void Login::Initialize(mojo::Connector* connector, const std::string& url,
                       uint32_t id, uint32_t user_id) {
  tracing_.Initialize(connector, url);

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  views::WindowManagerConnection::Create(connector);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new LoginView(this);

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
}

bool Login::AcceptConnection(mojo::Connection* connection) {
  if (connection->GetRemoteApplicationName() == "mojo:mash_init") {
    connection->GetInterface(&login_);
    return true;
  }
  return false;
}

}  // namespace login
}  // namespace main
