// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ATHENA_CHROME_BROWSER_MAIN_EXTRA_PARTS_ATHENA_H_
#define CHROME_BROWSER_UI_VIEWS_ATHENA_CHROME_BROWSER_MAIN_EXTRA_PARTS_ATHENA_H_

class ChromeBrowserMainExtraParts;

// Creates the BrowserMainExtraParts for athena environment. Note that athena
// and ash cannot coexist, and must be exclusive.
ChromeBrowserMainExtraParts* CreateChromeBrowserMainExtraPartsAthena();

#endif  // CHROME_BROWSER_UI_VIEWS_ATHENA_CHROME_BROWSER_MAIN_EXTRA_PARTS_ATHENA_H_
