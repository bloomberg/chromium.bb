// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UTILS_H_

class GURL;

// Returns true if the passed |url| refers to an internal chrome page.
bool InternalChromePage(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UTILS_H_
