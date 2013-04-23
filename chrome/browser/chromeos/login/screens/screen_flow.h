// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FLOW_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class ScreenContext;
class ScreenManager;

// Class that holds the logic of interpreting screen outcomes and moving between
// the screens.
class ScreenFlow {
 public:
  ScreenFlow();
  virtual ~ScreenFlow();

  // Called once the flow is set as a flow for specific screen manager.
  // Should usually result in get_manager()->ShowScreen call.
  virtual void Start() = 0;

  // Called when screen with |screen_id| finishes with |outcome|.
  // Additional result parameters may be passed via |context| (always not NULL).
  // Should result in get_manager()->ShowScreen call or in
  // get_manager()->SetScreenFlow call.
  virtual void OnScreenFinished(const std::string& screen_id,
                                const std::string& outcome,
                                ScreenContext* context) = 0;
 protected:
  ScreenManager* screen_manager() { return manager_; }

 private:
  friend class ScreenManager;

  void set_screen_manager(ScreenManager* manager) {
    manager_ = manager;
  }

  // Screen manager associated with this flow. Set by screen manager itself.
  ScreenManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ScreenFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FLOW_H_
