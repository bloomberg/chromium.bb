// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/login/login.h"

#include <map>
#include <memory>

#include "ash/public/interfaces/container.mojom.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/user_access_manager.mojom.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace login {
namespace {

class Login;

class UI : public views::WidgetDelegateView,
           public views::ButtonListener {
 public:
  static void Show(shell::Connector* connector,
                   const shell::Identity& identity,
                   Login* login) {
    UI* ui = new UI(login, connector);
    ui->StartWindowManager(identity);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = ui;

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[ash::mojom::kWindowContainer_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int32_t>(ash::mojom::Container::LOGIN_WINDOWS));
    ui::Window* window =
        views::WindowManagerConnection::Get()->NewWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget, window, ui::mojom::SurfaceType::DEFAULT);
    widget->Init(params);
    widget->Show();
  }

 private:
  UI(Login* login, shell::Connector* connector)
      : login_(login),
        connector_(connector),
        user_id_1_("00000000-0000-4000-8000-000000000000"),
        user_id_2_("00000000-0000-4000-8000-000000000001"),
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
  ~UI() override {
    // Prevent the window manager from restarting during graceful shutdown.
    mash_wm_connection_->SetConnectionLostClosure(base::Closure());
    base::MessageLoop::current()->QuitWhenIdle();
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Login");
  }
  void DeleteDelegate() override { delete this; }

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
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void StartWindowManager(const shell::Identity& identity) {
    mash_wm_connection_ = connector_->Connect("mojo:ash");
    mash_wm_connection_->SetConnectionLostClosure(
        base::Bind(&UI::StartWindowManager, base::Unretained(this), identity));
    window_manager_connection_ =
        views::WindowManagerConnection::Create(connector_, identity);
  }

  Login* login_;
  shell::Connector* connector_;
  const std::string user_id_1_;
  const std::string user_id_2_;
  views::LabelButton* login_button_1_;
  views::LabelButton* login_button_2_;
  std::unique_ptr<shell::Connection> mash_wm_connection_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

class Login : public shell::Service,
              public shell::InterfaceFactory<mojom::Login>,
              public mojom::Login {
 public:
   Login() {}
  ~Login() override {}

  void LoginAs(const std::string& user_id) {
    user_access_manager_->SetActiveUser(user_id);
    mash::init::mojom::InitPtr init;
    connector()->ConnectToInterface("mojo:mash_init", &init);
    init->StartService("mojo:mash_session", user_id);
  }

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    identity_ = identity;
    tracing_.Initialize(connector(), identity.name());

    aura_init_.reset(
        new views::AuraInit(connector(), "views_mus_resources.pak"));

    connector()->ConnectToInterface("mojo:ui", &user_access_manager_);
    user_access_manager_->SetActiveUser(identity.user_id());
  }
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<mojom::Login>(this);
    return true;
  }

  // shell::InterfaceFactory<mojom::Login>:
  void Create(const shell::Identity& remote_identity,
              mojom::LoginRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::Login:
  void ShowLoginUI() override {
    UI::Show(connector(), identity_, this);
  }
  void SwitchUser() override {
    UI::Show(connector(), identity_, this);
  }

  void StartWindowManager();

  shell::Identity identity_;
  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  mojo::BindingSet<mojom::Login> bindings_;
  ui::mojom::UserAccessManagerPtr user_access_manager_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

void UI::ButtonPressed(views::Button* sender, const ui::Event& event) {
  // Login...
  if (sender == login_button_1_) {
    login_->LoginAs(user_id_1_);
  } else if (sender == login_button_2_) {
    login_->LoginAs(user_id_2_);
  } else {
    NOTREACHED();
  }
  GetWidget()->Close();
}

}  // namespace

shell::Service* CreateLogin() {
  return new Login;
}

}  // namespace login
}  // namespace main
