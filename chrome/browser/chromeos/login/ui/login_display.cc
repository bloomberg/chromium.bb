// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display.h"

namespace chromeos {

LoginDisplay::Delegate::~Delegate() {}

LoginDisplay::LoginDisplay(Delegate* delegate,
                           const gfx::Rect& background_bounds)
    : delegate_(delegate),
      parent_window_(NULL),
      background_bounds_(background_bounds),
      is_signin_completed_(false) {
}

LoginDisplay::~LoginDisplay() {}

}  // namespace chromeos
