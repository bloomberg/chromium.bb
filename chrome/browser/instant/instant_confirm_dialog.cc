// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_confirm_dialog.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser {

void ShowInstantConfirmDialogIfNecessary(gfx::NativeWindow parent,
                                         Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  if (prefs->GetBoolean(prefs::kInstantConfirmDialogShown)) {
    InstantController::Enable(profile);
    return;
  }

  ShowInstantConfirmDialog(parent, profile);
}

}  // namespace browser
