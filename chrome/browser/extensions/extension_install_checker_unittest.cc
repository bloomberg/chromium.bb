// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_checker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/preload_check_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const PreloadCheck::Error kBlacklistError = PreloadCheck::BLACKLISTED_ID;
const char kDummyRequirementsError[] = "Requirements error";
const char kDummyPolicyError[] = "Cannot install extension";

}  // namespace

// Stubs most of the checks since we are interested in validating the logic in
// the install checker. This class implements a synchronous version of all
// checks.
class ExtensionInstallCheckerForTest : public ExtensionInstallChecker {
 public:
  ExtensionInstallCheckerForTest(int enabled_checks, bool fail_fast)
      : ExtensionInstallChecker(nullptr, nullptr, enabled_checks, fail_fast) {}

  ~ExtensionInstallCheckerForTest() override {}

  void set_requirements_error(const std::string& error) {
    requirements_error_ = error;
  }

  bool is_async() const { return is_async_; }
  void set_is_async(bool is_async) { is_async_ = is_async; }

 protected:
  void MockCheckRequirements() {
    if (!is_running())
      return;
    std::vector<std::string> errors;
    if (!requirements_error_.empty())
      errors.push_back(requirements_error_);
    OnRequirementsCheckDone(errors);
  }

  void CheckRequirements() override {
    if (is_async_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionInstallCheckerForTest::MockCheckRequirements,
                     base::Unretained(this)));
    } else {
      MockCheckRequirements();
    }
  }

 private:
  // Whether to run the requirements and blacklist checks asynchronously, as
  // they often do in ExtensionInstallChecker.
  bool is_async_ = false;

  // Dummy error for testing.
  std::string requirements_error_;
};

class CheckObserver {
 public:
  CheckObserver() : result_(0), call_count_(0) {}

  int result() const { return result_; }
  int call_count() const { return call_count_; }

  void OnChecksComplete(int checks_failed) {
    result_ = checks_failed;
    ++call_count_;
  }

  void Wait() {
    if (call_count_)
      return;

    base::RunLoop().RunUntilIdle();
  }

 private:
  int result_;
  int call_count_;
};

class ExtensionInstallCheckerTest : public testing::Test {
 public:
  ExtensionInstallCheckerTest() {}
  ~ExtensionInstallCheckerTest() override {}

  void SetBlacklistError(ExtensionInstallCheckerForTest* checker,
                         PreloadCheck::Error error) {
    auto blacklist_check = base::MakeUnique<PreloadCheckStub>();
    blacklist_check->set_is_async(checker->is_async());
    if (error != PreloadCheck::NONE)
      blacklist_check->AddError(error);
    checker->SetBlacklistCheckForTesting(std::move(blacklist_check));
  }

  void SetPolicyError(ExtensionInstallCheckerForTest* checker,
                      PreloadCheck::Error error,
                      std::string message) {
    // The policy check always runs synchronously.
    auto policy_check = base::MakeUnique<PreloadCheckStub>();
    if (error != PreloadCheck::NONE) {
      policy_check->AddError(error);
      policy_check->set_error_message(base::UTF8ToUTF16(message));
    }
    checker->SetPolicyCheckForTesting(std::move(policy_check));
  }

 protected:
  void SetAllPass(ExtensionInstallCheckerForTest* checker) {
    SetBlacklistError(checker, PreloadCheck::NONE);
    SetPolicyError(checker, PreloadCheck::NONE, "");
    checker->set_requirements_error("");
  }

  void SetAllErrors(ExtensionInstallCheckerForTest* checker) {
    SetBlacklistError(checker, kBlacklistError);
    SetPolicyError(checker, PreloadCheck::DISALLOWED_BY_POLICY,
                   kDummyPolicyError);
    checker->set_requirements_error(kDummyRequirementsError);
  }

  void ExpectRequirementsPass(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_TRUE(checker.requirement_errors().empty());
  }

  void ExpectRequirementsError(const char* expected_error,
                               const ExtensionInstallCheckerForTest& checker) {
    EXPECT_FALSE(checker.requirement_errors().empty());
    EXPECT_EQ(std::string(expected_error),
              checker.requirement_errors().front());
  }

  void ExpectRequirementsError(const ExtensionInstallCheckerForTest& checker) {
    ExpectRequirementsError(kDummyRequirementsError, checker);
  }

  void ExpectBlacklistPass(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_EQ(PreloadCheck::NONE, checker.blacklist_error());
  }

  void ExpectBlacklistError(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_EQ(kBlacklistError, checker.blacklist_error());
  }

  void ExpectPolicyPass(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_TRUE(checker.policy_error().empty());
  }

  void ExpectPolicyError(const char* expected_error,
                         const ExtensionInstallCheckerForTest& checker) {
    EXPECT_FALSE(checker.policy_error().empty());
    EXPECT_EQ(std::string(expected_error), checker.policy_error());
  }

  void ExpectPolicyError(const ExtensionInstallCheckerForTest& checker) {
    ExpectPolicyError(kDummyPolicyError, checker);
  }

  void RunChecker(ExtensionInstallCheckerForTest* checker,
                  int expected_result) {
    CheckObserver observer;
    checker->Start(base::Bind(&CheckObserver::OnChecksComplete,
                              base::Unretained(&observer)));
    observer.Wait();

    EXPECT_FALSE(checker->is_running());
    EXPECT_EQ(expected_result, observer.result());
    EXPECT_EQ(1, observer.call_count());
  }

  void DoRunAllChecksPass(ExtensionInstallCheckerForTest* checker) {
    SetAllPass(checker);
    RunChecker(checker,
               0);

    ExpectRequirementsPass(*checker);
    ExpectPolicyPass(*checker);
    ExpectBlacklistPass(*checker);
  }

  void DoRunAllChecksFail(ExtensionInstallCheckerForTest* checker) {
    SetAllErrors(checker);
    RunChecker(checker,
               ExtensionInstallChecker::CHECK_ALL);

    ExpectRequirementsError(*checker);
    ExpectPolicyError(*checker);
    ExpectBlacklistError(*checker);
  }

  void DoRunSubsetOfChecks(int checks_to_run) {
    ExtensionInstallCheckerForTest sync_checker(checks_to_run,
                                                /*fail_fast=*/false);
    ExtensionInstallCheckerForTest async_checker(checks_to_run,
                                                 /*fail_fast=*/false);
    async_checker.set_is_async(true);

    for (auto* checker : {&sync_checker, &async_checker}) {
      SetAllErrors(checker);
      RunChecker(checker, checks_to_run);

      if (checks_to_run & ExtensionInstallChecker::CHECK_REQUIREMENTS)
        ExpectRequirementsError(*checker);
      else
        ExpectRequirementsPass(*checker);

      if (checks_to_run & ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY)
        ExpectPolicyError(*checker);
      else
        ExpectPolicyPass(*checker);

      if (checks_to_run & ExtensionInstallChecker::CHECK_BLACKLIST)
        ExpectBlacklistError(*checker);
      else
        ExpectBlacklistPass(*checker);
    }
  }

 private:
  // A message loop is required for the asynchronous tests.
  base::MessageLoop message_loop;
};

// Test the case where all tests pass.
TEST_F(ExtensionInstallCheckerTest, AllSucceeded) {
  ExtensionInstallCheckerForTest sync_checker(
      ExtensionInstallChecker::CHECK_ALL, /*fail_fast=*/false);
  DoRunAllChecksPass(&sync_checker);

  ExtensionInstallCheckerForTest async_checker(
      ExtensionInstallChecker::CHECK_ALL, /*fail_fast=*/false);
  async_checker.set_is_async(true);
  DoRunAllChecksPass(&async_checker);
}

// Test the case where all tests fail.
TEST_F(ExtensionInstallCheckerTest, AllFailed) {
  ExtensionInstallCheckerForTest sync_checker(
      ExtensionInstallChecker::CHECK_ALL, /*fail_fast=*/false);
  DoRunAllChecksFail(&sync_checker);

  ExtensionInstallCheckerForTest async_checker(
      ExtensionInstallChecker::CHECK_ALL, /*fail_fast=*/false);
  async_checker.set_is_async(true);
  DoRunAllChecksFail(&async_checker);
}

// Test running only a subset of tests.
TEST_F(ExtensionInstallCheckerTest, RunSubsetOfChecks) {
  DoRunSubsetOfChecks(ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY |
                      ExtensionInstallChecker::CHECK_REQUIREMENTS);
  DoRunSubsetOfChecks(ExtensionInstallChecker::CHECK_BLACKLIST |
                      ExtensionInstallChecker::CHECK_REQUIREMENTS);
  DoRunSubsetOfChecks(ExtensionInstallChecker::CHECK_BLACKLIST);
}

// Test fail fast with synchronous callbacks.
TEST_F(ExtensionInstallCheckerTest, FailFastSync) {
  // This test assumes some internal knowledge of the implementation - that
  // the policy check runs first.
  {
    ExtensionInstallCheckerForTest checker(ExtensionInstallChecker::CHECK_ALL,
                                           /*fail_fast=*/true);
    SetAllErrors(&checker);
    RunChecker(&checker, ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY);

    ExpectRequirementsPass(checker);
    ExpectPolicyError(checker);
    ExpectBlacklistPass(checker);
  }

  {
    ExtensionInstallCheckerForTest checker(
        ExtensionInstallChecker::CHECK_REQUIREMENTS |
            ExtensionInstallChecker::CHECK_BLACKLIST,
        /*fail_fast=*/true);
    SetAllErrors(&checker);
    RunChecker(&checker, ExtensionInstallChecker::CHECK_REQUIREMENTS);

    ExpectRequirementsError(checker);
    ExpectPolicyPass(checker);
    ExpectBlacklistPass(checker);
  }
}

// Test fail fast with asynchronous callbacks.
TEST_F(ExtensionInstallCheckerTest, FailFastAsync) {
  // This test assumes some internal knowledge of the implementation - that
  // the requirements check runs before the blacklist check. Both checks should
  // be called, but the requirements check callback arrives first and the
  // blacklist result will be discarded.
  ExtensionInstallCheckerForTest checker(ExtensionInstallChecker::CHECK_ALL,
                                         /*fail_fast=*/true);
  checker.set_is_async(true);
  SetAllErrors(&checker);

  // The policy check is synchronous and needs to pass for the other tests to
  // run.
  SetPolicyError(&checker, PreloadCheck::NONE, "");

  RunChecker(&checker,
             ExtensionInstallChecker::CHECK_REQUIREMENTS);

  ExpectRequirementsError(checker);
  ExpectPolicyPass(checker);
  ExpectBlacklistPass(checker);
}

}  // namespace extensions
