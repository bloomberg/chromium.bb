// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_unit_test_suite.h"

#include "ios/public/test/test_chrome_browser_provider.h"
#include "ios/public/test/test_chrome_provider_initializer.h"
#include "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

namespace {

class IOSChromeUnitTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  IOSChromeUnitTestSuiteInitializer() {}
  ~IOSChromeUnitTestSuiteInitializer() override {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    web_client_.reset(new web::WebClient());
    web::SetWebClient(web_client_.get());
    test_ios_chrome_provider_initializer_.reset(
        new ios::TestChromeProviderInitializer());
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    web_client_.reset();
    web::SetWebClient(nullptr);
    test_ios_chrome_provider_initializer_.reset();
  }

 private:
  scoped_ptr<web::WebClient> web_client_;
  scoped_ptr<ios::TestChromeProviderInitializer>
      test_ios_chrome_provider_initializer_;
  DISALLOW_COPY_AND_ASSIGN(IOSChromeUnitTestSuiteInitializer);
};

}  // namespace

IOSChromeUnitTestSuite::IOSChromeUnitTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

IOSChromeUnitTestSuite::~IOSChromeUnitTestSuite() {
}

void IOSChromeUnitTestSuite::Initialize() {
  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new IOSChromeUnitTestSuiteInitializer);

  ui::RegisterPathProvider();

  {
    ios::TestChromeBrowserProvider provider;
    url::AddStandardScheme(provider.GetChromeUIScheme());
  }

  base::TestSuite::Initialize();
}
