// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_confirm_dialog.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

namespace browser {

// TODO: get the right url.
const char kInstantLearnMoreURL[] = "http://www.google.com";

void ShowInstantConfirmDialogIfNecessary(gfx::NativeWindow parent,
                                         Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  if (prefs->GetBoolean(prefs::kInstantConfirmDialogShown)) {
    prefs->SetBoolean(prefs::kInstantEnabled, true);
    return;
  }

  ShowInstantConfirmDialog(parent, profile);
}

#if !defined(TOOLKIT_VIEWS) && !defined(TOOLKIT_GTK)
void ShowInstantConfirmDialog(gfx::NativeWindow parent,
                              Profile* profile) {
  NOTIMPLEMENTED();
}
#endif

}  // namespace browser
