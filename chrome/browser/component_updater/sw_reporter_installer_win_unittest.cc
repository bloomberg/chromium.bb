// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class SwReporterInstallerTest : public ::testing::Test {
 public:
  SwReporterInstallerTest()
      : traits_(base::Bind(&SwReporterInstallerTest::SwReporterLaunched,
                           base::Unretained(this))),
        default_version_("1.2.3"),
        default_path_(L"C:\\full\\path\\to\\download") {}

 protected:
  // Each test fixture inherits from |SwReporterInstallerTest|, and friendship
  // is not transitive so they will not have access to |ComponentReady|. Use
  // this helper function to call it.
  void TestComponentReady(const base::Version& version,
                          const base::FilePath& path) {
    traits_.ComponentReady(version, path,
                           std::make_unique<base::DictionaryValue>());
  }

  void SwReporterLaunched(const safe_browsing::SwReporterInvocation& invocation,
                          const base::Version& version) {
    ASSERT_TRUE(launched_invocations_.empty());
    launched_invocations_.push_back(invocation);
    launched_version_ = version;
  }

  base::FilePath MakeTestFilePath(const base::FilePath& path) const {
    return path.Append(L"software_reporter_tool.exe");
  }

  // |ComponentReady| asserts that it is run on the UI thread, so we must
  // create test threads before calling it.
  content::TestBrowserThreadBundle threads_;

  // The traits object to test.
  SwReporterInstallerTraits traits_;

  // Default parameters for |ComponentReady|.
  base::Version default_version_;
  base::FilePath default_path_;

  // Results of running |ComponentReady|.
  std::vector<safe_browsing::SwReporterInvocation> launched_invocations_;
  base::Version launched_version_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerTest);
};

TEST_F(SwReporterInstallerTest, Default) {
  TestComponentReady(default_version_, default_path_);

  // The SwReporter should be launched exactly once, with no arguments.
  EXPECT_EQ(default_version_, launched_version_);
  ASSERT_EQ(1U, launched_invocations_.size());

  const safe_browsing::SwReporterInvocation& invocation =
      launched_invocations_[0];
  EXPECT_EQ(MakeTestFilePath(default_path_),
            invocation.command_line.GetProgram());
  EXPECT_TRUE(invocation.command_line.GetSwitches().empty());
  EXPECT_TRUE(invocation.command_line.GetArgs().empty());
}

}  // namespace component_updater
