// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_TEST_UTILS_H_
#define CHROME_BROWSER_INSTANT_INSTANT_TEST_UTILS_H_

#include <string>

#include "base/run_loop.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_model_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/common/search_types.h"
#include "chrome/test/base/in_process_browser_test.h"

class InstantTestModelObserver : public InstantModelObserver {
 public:
  InstantTestModelObserver(const InstantModel* model,
                           chrome::search::Mode::Type desired_mode_type);
  ~InstantTestModelObserver();

  void WaitUntilDesiredPreviewState();

  // Overridden from InstantModelObserver:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE;

 private:
  const InstantModel* const model_;
  const chrome::search::Mode::Type desired_mode_type_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InstantTestModelObserver);
};

class InstantTestBase : public InProcessBrowserTest {
 protected:
  void SetupInstant();
  void SetupInstantUsingTemplateURL();

  InstantController* instant() {
    return browser()->instant_controller()->instant();
  }

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetLocationEntry();
  }

  void KillInstantRenderView();

  void FocusOmnibox();
  void FocusOmniboxAndWaitForInstantSupport();

  void SetOmniboxText(const std::string& text);
  void SetOmniboxTextAndWaitForInstantToShow(const std::string& text);

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

  GURL instant_url_;
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_TEST_UTILS_H_
