// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// Mock TestLauncher to mock CreateAndStartThreadPool,
// unit test will provide a ScopedTaskEnvironment.
class MockTestLauncher : public TestLauncher {
 public:
  MockTestLauncher(TestLauncherDelegate* launcher_delegate,
                   size_t parallel_jobs)
      : TestLauncher(launcher_delegate, parallel_jobs) {}

  void CreateAndStartThreadPool(int parallel_jobs) override {}
};

// Simple TestLauncherDelegate mock to test TestLauncher flow.
class MockTestLauncherDelegate : public TestLauncherDelegate {
 public:
  MOCK_METHOD1(GetTests, bool(std::vector<TestIdentifier>* output));
  MOCK_METHOD2(WillRunTest,
               bool(const std::string& test_case_name,
                    const std::string& test_name));
  MOCK_METHOD2(ShouldRunTest,
               bool(const std::string& test_case_name,
                    const std::string& test_name));
  MOCK_METHOD2(RunTests,
               size_t(TestLauncher* test_launcher,
                      const std::vector<std::string>& test_names));
  MOCK_METHOD2(RetryTests,
               size_t(TestLauncher* test_launcher,
                      const std::vector<std::string>& test_names));
};

// Using MockTestLauncher to test TestLauncher.
// Test TestLauncher filters, and command line switches setup.
class TestLauncherTest : public testing::Test {
 protected:
  TestLauncherTest()
      : command_line(new CommandLine(CommandLine::NO_PROGRAM)),
        test_launcher(&delegate, 10),
        scoped_task_environment(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  // Posted task is needed to break RunLoop().Run() in TestLauncher.
  void SetUp() override {
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&RunLoop::QuitCurrentWhenIdleDeprecated));
  }

  // Setup expected delegate calls, and which tests the delegate will return.
  void SetUpExpectCalls(std::vector<const std::string> test_names) {
    std::vector<TestIdentifier> tests;
    for (const std::string& test_name : test_names) {
      TestIdentifier test_data;
      test_data.test_case_name = "Test";
      test_data.test_name = test_name;
      test_data.file = "File";
      test_data.line = 100;
      tests.push_back(test_data);
    }
    using ::testing::_;
    EXPECT_CALL(delegate, GetTests(_))
        .WillOnce(testing::DoAll(testing::SetArgPointee<0>(tests),
                                 testing::Return(true)));
    EXPECT_CALL(delegate, WillRunTest(_, _))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, ShouldRunTest(_, _))
        .WillRepeatedly(testing::Return(true));
  }

  std::unique_ptr<CommandLine> command_line;
  MockTestLauncher test_launcher;
  MockTestLauncherDelegate delegate;
  base::test::ScopedTaskEnvironment scoped_task_environment;
};

// Test TestLauncher filters DISABLED tests by default.
TEST_F(TestLauncherTest, FilterDisabledTestByDefault) {
  SetUpExpectCalls({"firstTest", "secondTest", "DISABLED_firstTest"});
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest", "Test.secondTest"};
  EXPECT_CALL(delegate,
              RunTests(_, testing::ElementsAreArray(tests_names.cbegin(),
                                                    tests_names.cend())))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher "gtest_filter" switch.
TEST_F(TestLauncherTest, UsingCommandLineSwitch) {
  SetUpExpectCalls({"firstTest", "secondTest", "DISABLED_firstTest"});
  command_line->AppendSwitchASCII("gtest_filter", "Test.first*");
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(delegate,
              RunTests(_, testing::ElementsAreArray(tests_names.cbegin(),
                                                    tests_names.cend())))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher run disabled unit tests switch.
TEST_F(TestLauncherTest, RunDisabledTests) {
  SetUpExpectCalls({"firstTest", "secondTest", "DISABLED_firstTest"});
  command_line->AppendSwitch("gtest_also_run_disabled_tests");
  command_line->AppendSwitchASCII("gtest_filter", "Test.first*");
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest",
                                          "Test.DISABLED_firstTest"};
  EXPECT_CALL(delegate,
              RunTests(_, testing::ElementsAreArray(tests_names.cbegin(),
                                                    tests_names.cend())))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, FaultyShardSetup) {
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "2");
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, RedirectStdio) {
  SetUpExpectCalls({});
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "always");
  using ::testing::_;
  EXPECT_CALL(delegate, RunTests(_, _)).Times(1);
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

}  // namespace

}  // namespace base
