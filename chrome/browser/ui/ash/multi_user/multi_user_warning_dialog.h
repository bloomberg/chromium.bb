// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WARNING_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WARNING_DIALOG_H_

#include "base/callback.h"

namespace chromeos {

// Creates and shows dialog with introduction.
void ShowMultiprofilesWarningDialog(const base::Callback<void(bool)> on_accept);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WARNING_DIALOG_H_
