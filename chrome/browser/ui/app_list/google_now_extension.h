// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_GOOGLE_NOW_EXTENSION_H_
#define CHROME_BROWSER_UI_APP_LIST_GOOGLE_NOW_EXTENSION_H_

#include <string>

// Returns true and sets |extension_id| if extension experiment enabled
//         false if no experiment or |extension_id| is empty.
bool GetGoogleNowExtensionId(std::string* extension_id);

#endif  // CHROME_BROWSER_UI_APP_LIST_GOOGLE_NOW_EXTENSION_H_
