// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_DISABLED_MAC_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_DISABLED_MAC_H_

class Profile;

// Register the handler to accept connections from the old app_list shim, and
// perform |action|; e.g. to open chrome://apps rather than the deleted App
// Launcher UI.
void InitAppsPageLegacyShimHandler(void (*action)(Profile*));

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_DISABLED_MAC_H_
