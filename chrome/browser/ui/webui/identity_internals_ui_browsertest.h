// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"

class IdentityInternalsUIBrowserTest : public WebUIBrowserTest {
 public:
  IdentityInternalsUIBrowserTest();
  ~IdentityInternalsUIBrowserTest() override;

 protected:
  void SetupTokenCache(int number_of_tokens);

  void SetupTokenCacheWithStoreApp();

 private:
  void AddTokenToCache(const std::string token_id,
                       const std::string extension_id,
                       const std::vector<std::string>& scopes,
                       int time_to_live);

  DISALLOW_COPY_AND_ASSIGN(IdentityInternalsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_
