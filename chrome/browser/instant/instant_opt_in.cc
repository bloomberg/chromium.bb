// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_opt_in.h"

#include "base/command_line.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"

namespace browser {

static bool dialog_shown = false;

bool ShouldShowInstantOptIn(Profile* profile) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowInstantOptIn))
    return false;

  // TODO(sky): implement me.
  return !dialog_shown;
}

void UserPickedInstantOptIn(gfx::NativeWindow parent,
                            Profile* profile,
                            bool opt_in) {
  // TODO: set pref so don't show opt-in again.
  dialog_shown = true;
  if (opt_in)
    browser::ShowInstantConfirmDialogIfNecessary(parent, profile);
}

}  // namespace browser
