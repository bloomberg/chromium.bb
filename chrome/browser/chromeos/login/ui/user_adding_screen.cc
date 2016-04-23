// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen_input_methods_controller.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

class UserAddingScreenImpl : public UserAddingScreen {
 public:
  void Start() override;
  void Cancel() override;
  bool IsRunning() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  static UserAddingScreenImpl* GetInstance();
 private:
  friend struct base::DefaultSingletonTraits<UserAddingScreenImpl>;

  void OnDisplayHostCompletion();

  UserAddingScreenImpl();
  ~UserAddingScreenImpl() override;

  base::ObserverList<Observer> observers_;
  LoginDisplayHost* display_host_;

  UserAddingScreenInputMethodsController im_controller_;
};

void UserAddingScreenImpl::Start() {
  CHECK(!IsRunning());
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  display_host_ = new chromeos::LoginDisplayHostImpl(screen_bounds);
  display_host_->StartUserAdding(
      base::Bind(&UserAddingScreenImpl::OnDisplayHostCompletion,
                 base::Unretained(this)));

  g_browser_process->platform_part()->SessionManager()->SetSessionState(
      session_manager::SESSION_STATE_LOGIN_SECONDARY);
  FOR_EACH_OBSERVER(Observer, observers_, OnUserAddingStarted());
}

void UserAddingScreenImpl::Cancel() {
  CHECK(IsRunning());

  // Make sure that system tray is enabled after this flow.
  ash::Shell::GetInstance()->GetPrimarySystemTray()->SetEnabled(true);
  display_host_->CancelUserAdding();

  // Reset wallpaper if cancel adding user from multiple user sign in page.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperManager::Get()->SetUserWallpaperDelayed(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  }
}

bool UserAddingScreenImpl::IsRunning() {
  return display_host_ != NULL;
}

void UserAddingScreenImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAddingScreenImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAddingScreenImpl::OnDisplayHostCompletion() {
  CHECK(IsRunning());
  display_host_ = NULL;

  g_browser_process->platform_part()->SessionManager()->SetSessionState(
      session_manager::SESSION_STATE_ACTIVE);
  FOR_EACH_OBSERVER(Observer, observers_, OnUserAddingFinished());
}

// static
UserAddingScreenImpl* UserAddingScreenImpl::GetInstance() {
  return base::Singleton<UserAddingScreenImpl>::get();
}

UserAddingScreenImpl::UserAddingScreenImpl()
    : display_host_(NULL), im_controller_(this) {
}

UserAddingScreenImpl::~UserAddingScreenImpl() {
}

}  // anonymous namespace

UserAddingScreen::UserAddingScreen() {}
UserAddingScreen::~UserAddingScreen() {}

UserAddingScreen* UserAddingScreen::Get() {
  return UserAddingScreenImpl::GetInstance();
}

}  // namespace chromeos

