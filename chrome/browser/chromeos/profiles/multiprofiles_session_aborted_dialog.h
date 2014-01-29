// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_SESSION_ABORTED_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_SESSION_ABORTED_DIALOG_H_

#include <string>

namespace chromeos {

// Shows a dialog when a user policy change ends a Multiprofile session.
void ShowMultiprofilesSessionAbortedDialog(const std::string& user_email);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_MULTIPROFILES_SESSION_ABORTED_DIALOG_H_
