// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_SCREEN_ORIENTATION_LISTENER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_SCREEN_ORIENTATION_LISTENER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/public/browser/sensors_listener.h"

// A singleton object to manage screen orientation.
class ScreenOrientationListener : public content::SensorsListener {
 public:
  // Returns the singleton object.
  static ScreenOrientationListener* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ScreenOrientationListener>;

  ScreenOrientationListener();
  virtual ~ScreenOrientationListener();

  // sensors::Listener implementation
  virtual void OnScreenOrientationChanged(
      content::ScreenOrientation change) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationListener);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_SCREEN_ORIENTATION_LISTENER_H_
