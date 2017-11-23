// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_

#include <string>

class Browser;
class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace local_ntp_test_utils {

content::WebContents* OpenNewTab(Browser* browser, const GURL& url);

// Switches the browser language to French, and returns true iff successful.
bool SwitchBrowserLanguageToFrench();

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url);

}  // namespace local_ntp_test_utils

#endif  // CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_
