// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class OmniboxView;

// This utility class is meant to be used in a "mix-in" fashion, giving the
// derived test class additional Instant-related functionality.
class InstantTestBase {
 protected:
  InstantTestBase();
  virtual ~InstantTestBase();

 protected:
  void SetupInstant(Browser* browser);
  void Init(const GURL& instant_url, const GURL& ntp_url,
            bool init_suggestions_url);

  void set_browser(Browser* browser) {
    browser_ = browser;
  }

  OmniboxView* omnibox() {
    return browser_->window()->GetLocationBar()->GetOmniboxView();
  }

  const GURL& instant_url() const { return instant_url_; }

  const GURL& ntp_url() const { return ntp_url_; }

  net::EmbeddedTestServer& https_test_server() { return https_test_server_; }

  void FocusOmnibox();

  void SetOmniboxText(const std::string& text);

  void PressEnterAndWaitForNavigation();
  void PressEnterAndWaitForFrameLoad();

  bool GetBoolFromJS(const content::ToRenderFrameHost& adapter,
                     const std::string& script,
                     bool* result) WARN_UNUSED_RESULT;
  bool GetIntFromJS(const content::ToRenderFrameHost& adapter,
                    const std::string& script,
                    int* result) WARN_UNUSED_RESULT;
  bool GetDoubleFromJS(const content::ToRenderFrameHost& adapter,
                       const std::string& script,
                       double* result) WARN_UNUSED_RESULT;
  bool GetStringFromJS(const content::ToRenderFrameHost& adapter,
                       const std::string& script,
                       std::string* result) WARN_UNUSED_RESULT;

  std::string GetOmniboxText();

  // Loads a named image from url |image| from the given |rvh| host.  |loaded|
  // returns whether the image was able to load without error.
  // The method returns true if the JavaScript executed cleanly.
  bool LoadImage(content::RenderViewHost* rvh,
                 const std::string& image,
                 bool* loaded);

 private:
  GURL instant_url_;
  GURL ntp_url_;

  Browser* browser_;

  // HTTPS Testing server, started on demand.
  net::EmbeddedTestServer https_test_server_;

  // Set to true to initialize suggestions URL in default search provider.
  bool init_suggestions_url_;

  DISALLOW_COPY_AND_ASSIGN(InstantTestBase);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_
