// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_

class Profile;

namespace first_run {

// Shows the first run dialog. Only called if IsOrganicFirstRun() is true.
// Returns true if the dialog was shown.
bool ShowFirstRunDialog(Profile* profile);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
