// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_
#define IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"

@class BrowserViewController;
@class BrowserViewControllerDependencyFactory;
@class TabModel;

// Base class for performance tests that require a browser view controller.  The
// BVC requires a non-trivial amount of setup and teardown, so it's best to
// derive from this class for tests that require a real BVC.  The class uses
// mock browser states and testing factories for AutocompleteClassifier.
class PerfTestWithBVC : public PerfTest {
 public:
  explicit PerfTestWithBVC(std::string testGroup);

  PerfTestWithBVC(std::string testGroup,
                  std::string firstLabel,
                  std::string averageLabel,
                  bool isWaterfall,
                  bool verbose,
                  bool slowTeardown,
                  int repeat);
  ~PerfTestWithBVC() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // True if the test needs extra time for Teardown.
  bool slow_teardown_;

  web::ScopedTestingWebClient web_client_;
  IOSChromeScopedTestingChromeBrowserProvider provider_;
  IOSChromeScopedTestingChromeBrowserStateManager browser_state_manager_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestChromeBrowserState> incognito_chrome_browser_state_;

  base::scoped_nsobject<TabModel> tab_model_;
  base::scoped_nsobject<TabModel> otr_tab_model_;

  base::scoped_nsobject<BrowserViewControllerDependencyFactory> bvc_factory_;
  base::scoped_nsobject<BrowserViewController> bvc_;
  base::scoped_nsobject<UIWindow> window_;
};

#endif  // IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_
