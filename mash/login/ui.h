// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_LOGIN_UI_H_
#define MASH_LOGIN_UI_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/user_access_manager.mojom.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget_delegate.h"

namespace mojo {
class Connection;
class Connector;
}

namespace views {
class AuraInit;
}

namespace mash {
namespace login {

class LoginController;

class UI : public views::WidgetDelegateView,
           public views::ButtonListener {
 public:
  static void Show(mojo::Connector* connector,
                   LoginController* login_controller);

 private:
  UI(LoginController* login_controller, mojo::Connector* connector);
  ~UI() override;

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;

  // Overridden from views::View:
  void Layout() override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void StartWindowManager();

  static bool is_showing_;
  LoginController* login_controller_;
  mojo::Connector* connector_;
  const std::string user_id_1_;
  const std::string user_id_2_;
  views::LabelButton* login_button_1_;
  views::LabelButton* login_button_2_;
  mus::mojom::UserAccessManagerPtr user_access_manager_;
  scoped_ptr<mojo::Connection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

}  // namespace login
}  // namespace mash

#endif  // MASH_LOGIN_UI_H_
