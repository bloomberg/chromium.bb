// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
#define ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

// Provides access to data needed by the lock/login screen. The login UI will
// register observers which are then invoked when data is posted to the data
// dispatcher.
//
// This provides access to data notification events only. LoginDataDispatcher is
// not responsible for owning data (the login embedder should own the data).
// This type provides a clean interface between the actual view/UI implemenation
// and the embedder.
//
// There are various types which provide data to LoginDataDispatcher. For
// example, the lock screen uses the session manager, whereas the login screen
// uses the user manager. The debug overlay proxies the original data dispatcher
// so it can provide fake state from an arbitrary source.
class ASH_EXPORT LoginDataDispatcher {
 public:
  // Types interested in login state should derive from |Observer| and register
  // themselves on the |LoginDataDispatcher| instance passed to the view
  // hierarchy.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the displayed set of users has changed.
    virtual void OnUsersChanged(
        const std::vector<mojom::LoginUserInfoPtr>& users);

    // Called when pin should be enabled or disabled for |user|. By default, pin
    // should be disabled.
    virtual void OnPinEnabledForUserChanged(const AccountId& user,
                                            bool enabled);

    // Called when the given user can click their pod to unlock.
    virtual void OnClickToUnlockEnabledForUserChanged(const AccountId& user,
                                                      bool enabled);

    // Called when the lock screen note state changes.
    virtual void OnLockScreenNoteStateChanged(mojom::TrayActionState state);

    // Called when an easy unlock icon should be displayed.
    virtual void OnShowEasyUnlockIcon(
        const AccountId& user,
        const mojom::EasyUnlockIconOptionsPtr& icon);
  };

  LoginDataDispatcher();
  ~LoginDataDispatcher();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyUsers(const std::vector<mojom::LoginUserInfoPtr>& users);
  void SetPinEnabledForUser(const AccountId& user, bool enabled);
  void SetClickToUnlockEnabledForUser(const AccountId& user, bool enabled);
  void SetLockScreenNoteState(mojom::TrayActionState state);
  void ShowEasyUnlockIcon(const AccountId& user,
                          const mojom::EasyUnlockIconOptionsPtr& icon);

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(LoginDataDispatcher);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
