// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_TEST_BASE_H_
#define ASH_LOGIN_UI_LOGIN_TEST_BASE_H_

#include <memory>

#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// Base test fixture for testing the views-based login and lock screens. This
// class provides easy access to types which the login/lock frequently need.
class LoginTestBase : public AshTestBase {
 public:
  LoginTestBase();
  ~LoginTestBase() override;

  // Shows a full Lock/Login screen. These methods are useful for when we want
  // to test interactions between multiple lock screen components, or when some
  // component needs to be able to talk directly to the lockscreen (e.g. getting
  // the ScreenType).
  void ShowLockScreen();
  void ShowLoginScreen();

  // Sets the primary test widget. The widget can be retrieved using |widget()|.
  // This can be used to make a widget scoped to the whole test, e.g. if the
  // widget is created in a SetUp override.
  // May be called at most once.
  void SetWidget(std::unique_ptr<views::Widget> widget);
  views::Widget* widget() const { return widget_.get(); }

  // Creates a widget containing |content|. The created widget will initially be
  // shown.
  std::unique_ptr<views::Widget> CreateWidgetWithContent(views::View* content);

  // Changes the active number of users. Fires an event on |data_dispatcher()|.
  void SetUserCount(size_t count);

  // Append number of |num_users| regular auth users.
  // Changes the active number of users. Fires an event on
  // |data_dispatcher()|.
  void AddUsers(size_t num_users);

  // Add a single user with the specified |email|.
  void AddUserByEmail(const std::string& email);

  // Append number of |num_public_accounts| public account users.
  // Changes the active number of users. Fires an event on
  // |data_dispatcher()|.
  void AddPublicAccountUsers(size_t num_public_accounts);

  std::vector<mojom::LoginUserInfoPtr>& users() { return users_; }

  const std::vector<mojom::LoginUserInfoPtr>& users() const { return users_; }

  // If the LockScreen is instantiated, returns its data dispatcher. Otherwise,
  // returns a standalone instance.
  // TODO(crbug/906676): rename this method to DataDispatcher.
  LoginDataDispatcher* data_dispatcher();

  // AshTestBase:
  void TearDown() override;

 private:
  class WidgetDelegate;

  // The widget created using |ShowWidgetWithContent|.
  std::unique_ptr<views::Widget> widget_;

  std::vector<mojom::LoginUserInfoPtr> users_;

  LoginDataDispatcher data_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(LoginTestBase);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_TEST_BASE_H_
