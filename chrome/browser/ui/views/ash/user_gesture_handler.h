// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_USER_GESTURE_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_USER_GESTURE_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/user_gesture_client.h"

class UserGestureHandler : public aura::client::UserGestureClient {
 public:
  UserGestureHandler();
  virtual ~UserGestureHandler();

  // aura::client::UserGestureClient overrides:
  virtual bool OnUserGesture(
      aura::client::UserGestureClient::Gesture gesture) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserGestureHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_USER_GESTURE_HANDLER_H_
