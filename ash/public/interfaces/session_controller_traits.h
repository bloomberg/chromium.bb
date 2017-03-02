// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_INTERFACES_SESSION_CONTROLLER_TRAITS_H_
#define ASH_PUBLIC_INTERFACES_SESSION_CONTROLLER_TRAITS_H_

#include "ash/public/cpp/session_types.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::SessionState, session_manager::SessionState> {
  static ash::mojom::SessionState ToMojom(session_manager::SessionState input) {
    switch (input) {
      case session_manager::SessionState::UNKNOWN:
        return ash::mojom::SessionState::UNKNOWN;
      case session_manager::SessionState::OOBE:
        return ash::mojom::SessionState::OOBE;
      case session_manager::SessionState::LOGIN_PRIMARY:
        return ash::mojom::SessionState::LOGIN_PRIMARY;
      case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
        return ash::mojom::SessionState::LOGGED_IN_NOT_ACTIVE;
      case session_manager::SessionState::ACTIVE:
        return ash::mojom::SessionState::ACTIVE;
      case session_manager::SessionState::LOCKED:
        return ash::mojom::SessionState::LOCKED;
      case session_manager::SessionState::LOGIN_SECONDARY:
        return ash::mojom::SessionState::LOGIN_SECONDARY;
    }
    NOTREACHED();
    return ash::mojom::SessionState::UNKNOWN;
  }

  static bool FromMojom(ash::mojom::SessionState input,
                        session_manager::SessionState* out) {
    switch (input) {
      case ash::mojom::SessionState::UNKNOWN:
        *out = session_manager::SessionState::UNKNOWN;
        return true;
      case ash::mojom::SessionState::OOBE:
        *out = session_manager::SessionState::OOBE;
        return true;
      case ash::mojom::SessionState::LOGIN_PRIMARY:
        *out = session_manager::SessionState::LOGIN_PRIMARY;
        return true;
      case ash::mojom::SessionState::LOGGED_IN_NOT_ACTIVE:
        *out = session_manager::SessionState::LOGGED_IN_NOT_ACTIVE;
        return true;
      case ash::mojom::SessionState::ACTIVE:
        *out = session_manager::SessionState::ACTIVE;
        return true;
      case ash::mojom::SessionState::LOCKED:
        *out = session_manager::SessionState::LOCKED;
        return true;
      case ash::mojom::SessionState::LOGIN_SECONDARY:
        *out = session_manager::SessionState::LOGIN_SECONDARY;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::UserType, user_manager::UserType> {
  static ash::mojom::UserType ToMojom(user_manager::UserType input) {
    switch (input) {
      case user_manager::USER_TYPE_REGULAR:
        return ash::mojom::UserType::REGULAR;
      case user_manager::USER_TYPE_GUEST:
        return ash::mojom::UserType::GUEST;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        return ash::mojom::UserType::PUBLIC_ACCOUNT;
      case user_manager::USER_TYPE_SUPERVISED:
        return ash::mojom::UserType::SUPERVISED;
      case user_manager::USER_TYPE_KIOSK_APP:
        return ash::mojom::UserType::KIOSK;
      case user_manager::USER_TYPE_CHILD:
        return ash::mojom::UserType::CHILD;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        return ash::mojom::UserType::ARC_KIOSK;
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        return ash::mojom::UserType::ACTIVE_DIRECTORY;
      case user_manager::NUM_USER_TYPES:
        // Bail as this is not a valid user type.
        break;
    }
    NOTREACHED();
    return ash::mojom::UserType::REGULAR;
  }

  static bool FromMojom(ash::mojom::UserType input,
                        user_manager::UserType* out) {
    switch (input) {
      case ash::mojom::UserType::REGULAR:
        *out = user_manager::USER_TYPE_REGULAR;
        return true;
      case ash::mojom::UserType::GUEST:
        *out = user_manager::USER_TYPE_GUEST;
        return true;
      case ash::mojom::UserType::PUBLIC_ACCOUNT:
        *out = user_manager::USER_TYPE_PUBLIC_ACCOUNT;
        return true;
      case ash::mojom::UserType::SUPERVISED:
        *out = user_manager::USER_TYPE_SUPERVISED;
        return true;
      case ash::mojom::UserType::KIOSK:
        *out = user_manager::USER_TYPE_KIOSK_APP;
        return true;
      case ash::mojom::UserType::CHILD:
        *out = user_manager::USER_TYPE_CHILD;
        return true;
      case ash::mojom::UserType::ARC_KIOSK:
        *out = user_manager::USER_TYPE_ARC_KIOSK_APP;
        return true;
      case ash::mojom::UserType::ACTIVE_DIRECTORY:
        *out = user_manager::USER_TYPE_ACTIVE_DIRECTORY;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::AddUserSessionPolicy, ash::AddUserSessionPolicy> {
  static ash::mojom::AddUserSessionPolicy ToMojom(
      ash::AddUserSessionPolicy input) {
    switch (input) {
      case ash::AddUserSessionPolicy::ALLOWED:
        return ash::mojom::AddUserSessionPolicy::ALLOWED;
      case ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
        return ash::mojom::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
      case ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
        return ash::mojom::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;
      case ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
        return ash::mojom::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;
    }
    NOTREACHED();
    return ash::mojom::AddUserSessionPolicy::ALLOWED;
  }

  static bool FromMojom(ash::mojom::AddUserSessionPolicy input,
                        ash::AddUserSessionPolicy* out) {
    switch (input) {
      case ash::mojom::AddUserSessionPolicy::ALLOWED:
        *out = ash::AddUserSessionPolicy::ALLOWED;
        return true;
      case ash::mojom::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
        *out = ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
        return true;
      case ash::mojom::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
        *out = ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;
        return true;
      case ash::mojom::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
        *out = ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::CycleUserDirection, ash::CycleUserDirection> {
  static ash::mojom::CycleUserDirection ToMojom(ash::CycleUserDirection input) {
    switch (input) {
      case ash::CycleUserDirection::NEXT:
        return ash::mojom::CycleUserDirection::NEXT;
      case ash::CycleUserDirection::PREVIOUS:
        return ash::mojom::CycleUserDirection::PREVIOUS;
    }
    NOTREACHED();
    return ash::mojom::CycleUserDirection::NEXT;
  }

  static bool FromMojom(ash::mojom::CycleUserDirection input,
                        ash::CycleUserDirection* out) {
    switch (input) {
      case ash::mojom::CycleUserDirection::NEXT:
        *out = ash::CycleUserDirection::NEXT;
        return true;
      case ash::mojom::CycleUserDirection::PREVIOUS:
        *out = ash::CycleUserDirection::PREVIOUS;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_INTERFACES_SESSION_CONTROLLER_TRAITS_H_
