// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
#pragma once

#include <string>

class Profile;

// Shows a dialog with more information about one click signin and waits for
// confirmation from the user.  The implementation of this function is platform
// specific.
void ShowOneClickSigninDialog(Profile* profile,
                              const std::string& session_index,
                              const std::string& email,
                              const std::string& password);

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
