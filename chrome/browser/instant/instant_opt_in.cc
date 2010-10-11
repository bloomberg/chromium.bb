// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_opt_in.h"

#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/profile.h"

namespace browser {

bool ShouldShowInstantOptIn(Profile* profile) {
  // TODO(sky): implement me.
  return false;
}

void UserPickedInstantOptIn(gfx::NativeWindow parent,
                            Profile* profile,
                            bool opt_in) {
  // TODO: set pref so don't show opt-in again.
  if (opt_in)
    browser::ShowInstantConfirmDialogIfNecessary(parent, profile);
}

}  // namespace browser
