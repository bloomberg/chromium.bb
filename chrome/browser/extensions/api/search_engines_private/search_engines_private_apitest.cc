// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/search_engines_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

class SearchEnginesPrivateApiTest : public ExtensionApiTest {
 public:
  SearchEnginesPrivateApiTest() {}
  ~SearchEnginesPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunSearchEnginesSubtest(const std::string& subtest) {
    return RunExtensionSubtest("search_engines_private",
                               "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateApiTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SearchEnginesPrivateApiTest,
                       DISABLED_SetSelectedSearchEngine) {
  EXPECT_TRUE(RunSearchEnginesSubtest("setSelectedSearchEngine")) << message_;
}

IN_PROC_BROWSER_TEST_F(SearchEnginesPrivateApiTest, OnSearchEnginesChanged) {
  EXPECT_TRUE(RunSearchEnginesSubtest("onDefaultSearchEnginesChanged"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SearchEnginesPrivateApiTest, AddNewSearchEngine) {
  EXPECT_TRUE(RunSearchEnginesSubtest("addNewSearchEngine")) << message_;
}

IN_PROC_BROWSER_TEST_F(SearchEnginesPrivateApiTest, UpdateSearchEngine) {
  EXPECT_TRUE(RunSearchEnginesSubtest("updateSearchEngine")) << message_;
}

IN_PROC_BROWSER_TEST_F(SearchEnginesPrivateApiTest, RemoveSearchEngine) {
  EXPECT_TRUE(RunSearchEnginesSubtest("removeSearchEngine")) << message_;
}

}  // namespace extensions
