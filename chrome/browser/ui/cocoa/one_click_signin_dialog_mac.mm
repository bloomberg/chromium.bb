// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/sync/one_click_signin_dialog.h"

void ShowOneClickSigninDialog(Profile* profile,
                              const std::string& email,
                              const std::string& password) {
  // TODO(rogerta): cocoa dialog not yet implemented.  See
  // one_click_signin_dialog_view.cc for what needs to be done here.
}
