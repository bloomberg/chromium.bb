// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CONTROLLER_H
#define COMPONENTS_PROXIMITY_AUTH_CONTROLLER_H

#include "base/macros.h"

namespace proximity_auth {

class Client;

// Main class that controls the logic flow of the Easy Unlock system.
class Controller {
 public:
  enum class State {
    NOT_STARTED,
    FINDING_CONNECTION,
    AUTHENTICATING,
    SECURE_CHANNEL_ESTABLISHED,
    AUTHENTICATION_FAILED,
    STOPPED,
  };

  virtual ~Controller() {}

  virtual State GetState() const = 0;
  virtual Client* GetClient() = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CONTROLLER_H
