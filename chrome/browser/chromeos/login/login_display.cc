// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_display.h"

namespace chromeos {

LoginDisplay::Delegate::~Delegate() {}

LoginDisplay::LoginDisplay(Delegate* delegate,
                           const gfx::Rect& background_bounds)
    : delegate_(delegate),
      parent_window_(NULL),
      background_bounds_(background_bounds) {
}

LoginDisplay::~LoginDisplay() {}

void LoginDisplay::Destroy() {
  delete this;
}

}  // namespace chromeos
