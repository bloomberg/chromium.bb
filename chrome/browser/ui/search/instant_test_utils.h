// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/instant_overlay_model_observer.h"
#include "chrome/common/search_types.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"

class InstantController;
class InstantModel;
class OmniboxView;

namespace content {
class WebContents;
};

class InstantTestModelObserver : public InstantOverlayModelObserver {
 public:
  InstantTestModelObserver(InstantOverlayModel* model,
                           chrome::search::Mode::Type desired_mode_type);
  ~InstantTestModelObserver();

  void WaitForDesiredOverlayState();

  // Overridden from InstantOverlayModelObserver:
  virtual void OverlayStateChanged(const InstantOverlayModel& model) OVERRIDE;

 private:
  InstantOverlayModel* const model_;
  const chrome::search::Mode::Type desired_mode_type_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InstantTestModelObserver);
};

// This utility class is meant to be used in a "mix-in" fashion, giving the
// derived test class additional Instant-related functionality.
class InstantTestBase {
 protected:
  InstantTestBase()
      : https_test_server_(
            net::TestServer::TYPE_HTTPS,
            net::BaseTestServer::SSLOptions(),
            base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }
  virtual ~InstantTestBase() {}

 protected:
  void SetupInstant(Browser* browser);
  void Init(const GURL& instant_url);

  void SetInstantURL(const std::string& url);

  void set_browser(Browser* browser) {
    browser_ = browser;
  }

  InstantController* instant() {
    return browser_->instant_controller()->instant();
  }

  OmniboxView* omnibox() {
    return browser_->window()->GetLocationBar()->GetLocationEntry();
  }

  const GURL& instant_url() const { return instant_url_; }

  net::TestServer& https_test_server() { return https_test_server_; }

  void KillInstantRenderView();

  void FocusOmnibox();
  void FocusOmniboxAndWaitForInstantSupport();
  void FocusOmniboxAndWaitForInstantExtendedSupport();

  void SetOmniboxText(const std::string& text);
  void SetOmniboxTextAndWaitForOverlayToShow(const std::string& text);
  void SetOmniboxTextAndWaitForSuggestion(const std::string& text);

  bool GetBoolFromJS(content::WebContents* contents,
                     const std::string& script,
                     bool* result) WARN_UNUSED_RESULT;
  bool GetIntFromJS(content::WebContents* contents,
                    const std::string& script,
                    int* result) WARN_UNUSED_RESULT;
  bool GetStringFromJS(content::WebContents* contents,
                       const std::string& script,
                       std::string* result) WARN_UNUSED_RESULT;
  bool ExecuteScript(const std::string& script) WARN_UNUSED_RESULT;
  bool CheckVisibilityIs(content::WebContents* contents,
                         bool expected) WARN_UNUSED_RESULT;
  bool HasUserInputInProgress();
  bool HasTemporaryText();

  // Loads a named image from url |image| from the given |rvh| host.  |loaded|
  // returns whether the image was able to load without error.
  // The method returns true if the JavaScript executed cleanly.
  bool LoadImage(content::RenderViewHost* rvh,
                 const std::string& image,
                 bool* loaded);

 private:
  GURL instant_url_;

  Browser* browser_;

  // HTTPS Testing server, started on demand.
  net::TestServer https_test_server_;

  DISALLOW_COPY_AND_ASSIGN(InstantTestBase);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_UTILS_H_
