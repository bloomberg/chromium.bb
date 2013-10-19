// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

const char kHostedAppId[] = "dceacbkfkmllgmjmbhgkpjegnodmildf";
const char kPackagedApp1Id[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

class AppSearchProviderTest : public ExtensionServiceTestBase {
 public:
  AppSearchProviderTest() {}
  virtual ~AppSearchProviderTest() {}

  // ExtensionServiceTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();

    // Load "app_list" extensions test profile.
    // The test profile has 4 extensions:
    // 1 dummy extension, 2 packaged extension apps and 1 hosted extension app.
    base::FilePath source_install_dir = data_dir_
        .AppendASCII("app_list")
        .AppendASCII("Extensions");
    base::FilePath pref_path = source_install_dir
        .DirName()
        .AppendASCII("Preferences");
    InitializeInstalledExtensionService(pref_path, source_install_dir);
    service_->Init();

    // There should be 4 extensions in the test profile.
    const ExtensionSet* extensions = service_->extensions();
    ASSERT_EQ(static_cast<size_t>(4),  extensions->size());

    app_search_.reset(new AppSearchProvider(profile_.get(), NULL));
  }

  std::string RunQuery(const std::string& query) {
    app_search_->Start(UTF8ToUTF16(query));
    app_search_->Stop();

    std::string result_str;
    const SearchProvider::Results& results = app_search_->results();
    for (size_t i = 0; i < results.size(); ++i) {
      if (!result_str.empty())
        result_str += ',';

      result_str += UTF16ToUTF8(results[i]->title());
    }
    return result_str;
  }

 private:
  scoped_ptr<AppSearchProvider> app_search_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
  EXPECT_EQ("", RunQuery(""));
  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  const std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_EQ("Packaged App 2", RunQuery("pa2"));
  EXPECT_EQ("Packaged App 1", RunQuery("app1"));
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, Uninstall) {
  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  service_->UninstallExtension(kPackagedApp1Id, false, NULL);
  EXPECT_EQ("", RunQuery("pa1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace app_list
