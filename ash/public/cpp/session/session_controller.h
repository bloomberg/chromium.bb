// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_

#include <stdint.h>

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_types.h"

namespace ash {

class SessionControllerClient;

// Interface to manage user sessions in ash.
class ASH_PUBLIC_EXPORT SessionController {
 public:
  // Get the instance of session controller.
  static SessionController* Get();

  // Sets the client interface.
  virtual void SetClient(SessionControllerClient* client) = 0;

  // Sets the ash session info.
  virtual void SetSessionInfo(const SessionInfo& info) = 0;

  // Updates a user session. This is called when a user session is added or
  // its meta data (e.g. name, avatar) is changed. There is no method to remove
  // a user session because ash/chrome does not support that. All users are
  // logged out at the same time.
  virtual void UpdateUserSession(const UserSession& user_session) = 0;

  // Sets the order of user sessions. The order is keyed by the session id.
  // Currently, session manager set a LRU order with the first one being the
  // active user session.
  virtual void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_ids) = 0;

 protected:
  SessionController();
  virtual ~SessionController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
