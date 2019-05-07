// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class SessionControllerClient;

// Interface to manage user sessions in ash.
class ASH_PUBLIC_EXPORT SessionController {
 public:
  // Get the instance of session controller.
  static SessionController* Get();

  // Sets the client interface.
  virtual void SetClient(SessionControllerClient* client) = 0;

 protected:
  SessionController();
  virtual ~SessionController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
