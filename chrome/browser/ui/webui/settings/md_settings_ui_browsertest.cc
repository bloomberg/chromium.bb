// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

typedef InProcessBrowserTest MdSettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(MdSettingsBrowserTest, ViewSourceDoesntCrash) {
  ui_test_utils::NavigateToURL(browser(),
      GURL(content::kViewSourceScheme + std::string(":") +
           chrome::kChromeUIMdSettingsURL));
}
