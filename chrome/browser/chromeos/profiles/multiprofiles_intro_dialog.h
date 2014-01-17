// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_INTRO_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_INTRO_DIALOG_H_

#include "base/callback.h"

namespace chromeos {

// Creates and shows dialog with introduction.
void ShowMultiprofilesIntroDialog(const base::Callback<void(bool)> on_accept);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_INTRO_DIALOG_H_
