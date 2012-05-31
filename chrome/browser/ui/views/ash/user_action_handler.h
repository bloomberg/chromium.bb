// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_USER_ACTION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_USER_ACTION_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/user_action_client.h"

class UserActionHandler : public aura::client::UserActionClient {
 public:
  UserActionHandler();
  virtual ~UserActionHandler();

  // aura::client::UserActionClient overrides:
  virtual bool OnUserAction(
      aura::client::UserActionClient::Command command) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserActionHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_USER_ACTION_HANDLER_H_
