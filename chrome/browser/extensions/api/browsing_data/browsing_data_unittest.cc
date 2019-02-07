// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace extensions {

namespace {

enum OriginTypeMask {
  UNPROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
  PROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
  EXTENSION = ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION
};

}

class BrowsingDataApiTest : public ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_TABBED;
    params.window = browser_window_.get();
    browser_ = std::make_unique<Browser>(params);

    remover_ = content::BrowserContext::GetBrowsingDataRemover(profile());
    remover_->SetEmbedderDelegate(&delegate_);
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  void CheckInvalidRemovalArgs(const std::string& args,
                               const std::string& expected_error) {
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    EXPECT_EQ(RunFunctionAndReturnError(function.get(), args, browser()),
              expected_error)
        << " for " << args;
  }

  void VerifyFilterBuilder(
      const std::string& options,
      const content::BrowsingDataFilterBuilder& filter_builder) {
    delegate()->ExpectCall(
        base::Time::UnixEpoch(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, UNPROTECTED_WEB,
        filter_builder);
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    EXPECT_EQ(RunFunctionAndReturnSingleResult(
                  function.get(), "[" + options + ", {\"localStorage\": true}]",
                  browser()),
              nullptr)
        << options;
    delegate()->VerifyAndClearExpectations();
  }

  Browser* browser() { return browser_.get(); }

  content::MockBrowsingDataRemoverDelegate* delegate() { return &delegate_; }

 private:
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  content::BrowsingDataRemover* remover_;
  content::MockBrowsingDataRemoverDelegate delegate_;
};

TEST_F(BrowsingDataApiTest, RemoveWithoutFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::BLACKLIST);
  ASSERT_TRUE(filter_builder->IsEmptyBlacklist());

  VerifyFilterBuilder("{}", *filter_builder);
}

TEST_F(BrowsingDataApiTest, RemoveWithWhitelistFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(R"({"origins": ["http://example.com"]})",
                      *filter_builder);
}

TEST_F(BrowsingDataApiTest, RemoveWithBlacklistFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::BLACKLIST);
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(R"({"excludeOrigins": ["http://example.com"]})",
                      *filter_builder);
}

TEST_F(BrowsingDataApiTest, RemoveWithSpecialUrlFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::BLACKLIST);
  filter_builder->AddOrigin(url::Origin::Create(GURL("file:///")));
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(
      R"({"excludeOrigins": ["file:///tmp/foo.html/",
          "filesystem:http://example.com/foo.txt"]})",
      *filter_builder);
}

TEST_F(BrowsingDataApiTest, RemoveCookiesWithFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::BLACKLIST);
  filter_builder->AddRegisterableDomain("example.com");
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                         UNPROTECTED_WEB, *filter_builder);

  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(RunFunctionAndReturnSingleResult(
                function.get(),
                R"([{"excludeOrigins": ["http://example.com"]},
                    {"cookies": true}])",
                browser()),
            nullptr);
  delegate()->VerifyAndClearExpectations();
}

// Expect two separate BrowsingDataRemover calls if cookies and localstorage
// are passed with a filter.
TEST_F(BrowsingDataApiTest, RemoveCookiesAndStorageWithFilter) {
  auto filter_builder1 = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::WHITELIST);
  filter_builder1->AddRegisterableDomain("example.com");
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                         UNPROTECTED_WEB, *filter_builder1);

  auto filter_builder2 = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::WHITELIST);
  filter_builder2->AddOrigin(
      url::Origin::Create(GURL("http://www.example.com")));
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                         UNPROTECTED_WEB, *filter_builder2);

  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(RunFunctionAndReturnSingleResult(
                function.get(),
                R"([{"origins": ["http://www.example.com"]},
                    {"cookies": true, "localStorage": true}])",
                browser()),
            nullptr);
  delegate()->VerifyAndClearExpectations();
}

TEST_F(BrowsingDataApiTest, RemoveWithFilterAndInvalidParameters) {
  CheckInvalidRemovalArgs(
      R"([{"origins": ["http://example.com"]}, {"passwords": true}])",
      extension_browsing_data_api_constants::kNonFilterableError);

  CheckInvalidRemovalArgs(
      R"([{"origins": ["http://example.com"],
          "excludeOrigins": ["https://example.com"]},
          {"localStorage": true}])",
      extension_browsing_data_api_constants::kIncompatibleFilterError);

  CheckInvalidRemovalArgs(
      R"([{"origins": ["foo"]}, {"localStorage": true}])",
      base::StringPrintf(
          extension_browsing_data_api_constants::kInvalidOriginError, "foo"));

  CheckInvalidRemovalArgs(
      R"([{"excludeOrigins": ["foo"]}, {"localStorage": true}])",
      base::StringPrintf(
          extension_browsing_data_api_constants::kInvalidOriginError, "foo"));
}
}  // namespace extensions
