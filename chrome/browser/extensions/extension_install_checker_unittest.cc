// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const BlacklistState kBlacklistStateError = BLACKLISTED_MALWARE;
const char kDummyRequirementsError[] = "Requirements error";
const char kDummyPolicyError[] = "Cannot install extension";

const char kDummyPolicyError2[] = "Another policy error";
const char kDummyRequirementsError2[] = "Another requirements error";
const BlacklistState kBlacklistState2 = BLACKLISTED_SECURITY_VULNERABILITY;

}  // namespace

// Stubs most of the checks since we are interested in validating the logic in
// the install checker. This class implements a synchronous version of all
// checks.
class ExtensionInstallCheckerForTest : public ExtensionInstallChecker {
 public:
  ExtensionInstallCheckerForTest()
      : ExtensionInstallChecker(NULL),
        requirements_check_called_(false),
        blacklist_check_called_(false),
        policy_check_called_(false),
        blacklist_state_(NOT_BLACKLISTED) {}

  virtual ~ExtensionInstallCheckerForTest() {}

  void set_requirements_error(const std::string& error) {
    requirements_error_ = error;
  }
  void set_policy_check_error(const std::string& error) {
    policy_check_error_ = error;
  }
  void set_blacklist_state(BlacklistState state) { blacklist_state_ = state; }

  bool requirements_check_called() const { return requirements_check_called_; }
  bool blacklist_check_called() const { return blacklist_check_called_; }
  bool policy_check_called() const { return policy_check_called_; }

  void MockCheckRequirements(int sequence_number) {
    std::vector<std::string> errors;
    if (!requirements_error_.empty())
      errors.push_back(requirements_error_);
    OnRequirementsCheckDone(sequence_number, errors);
  }

  void MockCheckBlacklistState(int sequence_number) {
    OnBlacklistStateCheckDone(sequence_number, blacklist_state_);
  }

 protected:
  virtual void CheckRequirements() OVERRIDE {
    requirements_check_called_ = true;
    MockCheckRequirements(current_sequence_number());
  }

  virtual void CheckManagementPolicy() OVERRIDE {
    policy_check_called_ = true;
    OnManagementPolicyCheckDone(policy_check_error_.empty(),
                                policy_check_error_);
  }

  virtual void CheckBlacklistState() OVERRIDE {
    blacklist_check_called_ = true;
    MockCheckBlacklistState(current_sequence_number());
  }

  virtual void ResetResults() OVERRIDE {
    ExtensionInstallChecker::ResetResults();

    requirements_check_called_ = false;
    blacklist_check_called_ = false;
    policy_check_called_ = false;
  }

  bool requirements_check_called_;
  bool blacklist_check_called_;
  bool policy_check_called_;

  // Dummy errors for testing.
  std::string requirements_error_;
  std::string policy_check_error_;
  BlacklistState blacklist_state_;
};

// This class implements asynchronous mocks of the requirements and blacklist
// checks.
class ExtensionInstallCheckerAsync : public ExtensionInstallCheckerForTest {
 protected:
  virtual void CheckRequirements() OVERRIDE {
    requirements_check_called_ = true;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstallCheckerForTest::MockCheckRequirements,
                   base::Unretained(this),
                   current_sequence_number()));
  }

  virtual void CheckBlacklistState() OVERRIDE {
    blacklist_check_called_ = true;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstallCheckerForTest::MockCheckBlacklistState,
                   base::Unretained(this),
                   current_sequence_number()));
  }
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
  virtual ~ExtensionInstallCheckerTest() {}

  void RunSecondInvocation(ExtensionInstallCheckerForTest* checker,
                           int checks_failed) {
    EXPECT_GT(checks_failed, 0);
    EXPECT_FALSE(checker->is_running());
    ValidateExpectedCalls(ExtensionInstallChecker::CHECK_ALL, *checker);

    // Set up different return values.
    checker->set_blacklist_state(kBlacklistState2);
    checker->set_policy_check_error(kDummyPolicyError2);
    checker->set_requirements_error(kDummyRequirementsError2);

    // Run the install checker again and ensure the second set of return values
    // is received.
    checker->Start(
        ExtensionInstallChecker::CHECK_ALL,
        false /* fail fast */,
        base::Bind(&ExtensionInstallCheckerTest::ValidateSecondInvocation,
                   base::Unretained(this),
                   checker));
  }

  void ValidateSecondInvocation(ExtensionInstallCheckerForTest* checker,
                                int checks_failed) {
    EXPECT_FALSE(checker->is_running());
    EXPECT_EQ(ExtensionInstallChecker::CHECK_REQUIREMENTS |
                  ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY,
              checks_failed);
    ValidateExpectedCalls(ExtensionInstallChecker::CHECK_ALL, *checker);

    EXPECT_EQ(kBlacklistState2, checker->blacklist_state());
    ExpectPolicyError(kDummyPolicyError2, *checker);
    ExpectRequirementsError(kDummyRequirementsError2, *checker);
  }

 protected:
  void SetAllErrors(ExtensionInstallCheckerForTest* checker) {
    checker->set_blacklist_state(kBlacklistStateError);
    checker->set_policy_check_error(kDummyPolicyError);
    checker->set_requirements_error(kDummyRequirementsError);
  }

  void ValidateExpectedCalls(int call_mask,
                             const ExtensionInstallCheckerForTest& checker) {
    bool expect_blacklist_checked =
        (call_mask & ExtensionInstallChecker::CHECK_BLACKLIST) != 0;
    bool expect_requirements_checked =
        (call_mask & ExtensionInstallChecker::CHECK_REQUIREMENTS) != 0;
    bool expect_policy_checked =
        (call_mask & ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY) != 0;
    EXPECT_EQ(expect_blacklist_checked, checker.blacklist_check_called());
    EXPECT_EQ(expect_policy_checked, checker.policy_check_called());
    EXPECT_EQ(expect_requirements_checked, checker.requirements_check_called());
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
    EXPECT_EQ(NOT_BLACKLISTED, checker.blacklist_state());
  }

  void ExpectBlacklistError(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_EQ(kBlacklistStateError, checker.blacklist_state());
  }

  void ExpectPolicyPass(const ExtensionInstallCheckerForTest& checker) {
    EXPECT_TRUE(checker.policy_allows_load());
    EXPECT_TRUE(checker.policy_error().empty());
  }

  void ExpectPolicyError(const char* expected_error,
                         const ExtensionInstallCheckerForTest& checker) {
    EXPECT_FALSE(checker.policy_allows_load());
    EXPECT_FALSE(checker.policy_error().empty());
    EXPECT_EQ(std::string(expected_error), checker.policy_error());
  }

  void ExpectPolicyError(const ExtensionInstallCheckerForTest& checker) {
    ExpectPolicyError(kDummyPolicyError, checker);
  }

  void RunChecker(ExtensionInstallCheckerForTest* checker,
                  bool fail_fast,
                  int checks_to_run,
                  int expected_checks_run,
                  int expected_result) {
    CheckObserver observer;
    checker->Start(checks_to_run,
                   fail_fast,
                   base::Bind(&CheckObserver::OnChecksComplete,
                              base::Unretained(&observer)));
    observer.Wait();

    EXPECT_FALSE(checker->is_running());
    EXPECT_EQ(expected_result, observer.result());
    EXPECT_EQ(1, observer.call_count());
    ValidateExpectedCalls(expected_checks_run, *checker);
  }

  void DoRunAllChecksPass(ExtensionInstallCheckerForTest* checker) {
    RunChecker(checker,
               false /* fail fast */,
               ExtensionInstallChecker::CHECK_ALL,
               ExtensionInstallChecker::CHECK_ALL,
               0);

    ExpectRequirementsPass(*checker);
    ExpectPolicyPass(*checker);
    ExpectBlacklistPass(*checker);
  }

  void DoRunAllChecksFail(ExtensionInstallCheckerForTest* checker) {
    SetAllErrors(checker);
    RunChecker(checker,
               false /* fail fast */,
               ExtensionInstallChecker::CHECK_ALL,
               ExtensionInstallChecker::CHECK_ALL,
               ExtensionInstallChecker::CHECK_ALL);

    ExpectRequirementsError(*checker);
    ExpectPolicyError(*checker);
    ExpectBlacklistError(*checker);
  }

  void DoRunSubsetOfChecks(ExtensionInstallCheckerForTest* checker) {
    // Test check set 1.
    int tests_to_run = ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY |
                       ExtensionInstallChecker::CHECK_REQUIREMENTS;
    SetAllErrors(checker);
    RunChecker(checker, false, tests_to_run, tests_to_run, tests_to_run);

    ExpectRequirementsError(*checker);
    ExpectPolicyError(*checker);
    ExpectBlacklistPass(*checker);

    // Test check set 2.
    tests_to_run = ExtensionInstallChecker::CHECK_BLACKLIST |
                   ExtensionInstallChecker::CHECK_REQUIREMENTS;
    SetAllErrors(checker);
    RunChecker(checker, false, tests_to_run, tests_to_run, tests_to_run);

    ExpectRequirementsError(*checker);
    ExpectPolicyPass(*checker);
    ExpectBlacklistError(*checker);

    // Test a single check.
    tests_to_run = ExtensionInstallChecker::CHECK_BLACKLIST;
    SetAllErrors(checker);
    RunChecker(checker, false, tests_to_run, tests_to_run, tests_to_run);

    ExpectRequirementsPass(*checker);
    ExpectPolicyPass(*checker);
    ExpectBlacklistError(*checker);
  }

 private:
  // A message loop is required for the asynchronous tests.
  base::MessageLoop message_loop;
};

class ExtensionInstallCheckerMultipleInvocationTest
    : public ExtensionInstallCheckerTest {
 public:
  ExtensionInstallCheckerMultipleInvocationTest() : callback_count_(0) {}
  virtual ~ExtensionInstallCheckerMultipleInvocationTest() {}

  void RunSecondInvocation(ExtensionInstallCheckerForTest* checker,
                           int checks_failed) {
    ASSERT_EQ(0, callback_count_);
    ++callback_count_;
    EXPECT_FALSE(checker->is_running());
    EXPECT_GT(checks_failed, 0);
    ValidateExpectedCalls(ExtensionInstallChecker::CHECK_ALL, *checker);

    // Set up different return values.
    checker->set_blacklist_state(kBlacklistState2);
    checker->set_policy_check_error(kDummyPolicyError2);
    checker->set_requirements_error(kDummyRequirementsError2);

    // Run the install checker again and ensure the second set of return values
    // is received.
    checker->Start(ExtensionInstallChecker::CHECK_ALL,
                   false /* fail fast */,
                   base::Bind(&ExtensionInstallCheckerMultipleInvocationTest::
                                  ValidateSecondInvocation,
                              base::Unretained(this),
                              checker));
  }

  void ValidateSecondInvocation(ExtensionInstallCheckerForTest* checker,
                                int checks_failed) {
    ASSERT_EQ(1, callback_count_);
    EXPECT_FALSE(checker->is_running());
    EXPECT_EQ(ExtensionInstallChecker::CHECK_REQUIREMENTS |
                  ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY,
              checks_failed);
    ValidateExpectedCalls(ExtensionInstallChecker::CHECK_ALL, *checker);

    EXPECT_EQ(kBlacklistState2, checker->blacklist_state());
    ExpectPolicyError(kDummyPolicyError2, *checker);
    ExpectRequirementsError(kDummyRequirementsError2, *checker);
  }

 private:
  int callback_count_;
};

// Test the case where all tests pass.
TEST_F(ExtensionInstallCheckerTest, AllSucceeded) {
  ExtensionInstallCheckerForTest sync_checker;
  DoRunAllChecksPass(&sync_checker);

  ExtensionInstallCheckerAsync async_checker;
  DoRunAllChecksPass(&async_checker);
}

// Test the case where all tests fail.
TEST_F(ExtensionInstallCheckerTest, AllFailed) {
  ExtensionInstallCheckerForTest sync_checker;
  DoRunAllChecksFail(&sync_checker);

  ExtensionInstallCheckerAsync async_checker;
  DoRunAllChecksFail(&async_checker);
}

// Test running only a subset of tests.
TEST_F(ExtensionInstallCheckerTest, RunSubsetOfChecks) {
  ExtensionInstallCheckerForTest sync_checker;
  ExtensionInstallCheckerAsync async_checker;
  DoRunSubsetOfChecks(&sync_checker);
  DoRunSubsetOfChecks(&async_checker);
}

// Test fail fast with synchronous callbacks.
TEST_F(ExtensionInstallCheckerTest, FailFastSync) {
  // This test assumes some internal knowledge of the implementation - that
  // the policy check runs first.
  ExtensionInstallCheckerForTest checker;
  SetAllErrors(&checker);
  RunChecker(&checker,
             true /* fail fast */,
             ExtensionInstallChecker::CHECK_ALL,
             ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY,
             ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY);

  ExpectRequirementsPass(checker);
  ExpectPolicyError(checker);
  ExpectBlacklistPass(checker);

  // This test assumes some internal knowledge of the implementation - that
  // the requirements check runs before the blacklist check.
  SetAllErrors(&checker);
  RunChecker(&checker,
             true /* fail fast */,
             ExtensionInstallChecker::CHECK_REQUIREMENTS |
                 ExtensionInstallChecker::CHECK_BLACKLIST,
             ExtensionInstallChecker::CHECK_REQUIREMENTS,
             ExtensionInstallChecker::CHECK_REQUIREMENTS);

  ExpectRequirementsError(checker);
  ExpectPolicyPass(checker);
  ExpectBlacklistPass(checker);
}

// Test fail fast with asynchronous callbacks.
TEST_F(ExtensionInstallCheckerTest, FailFastAsync) {
  // This test assumes some internal knowledge of the implementation - that
  // the requirements check runs before the blacklist check. Both checks should
  // be called, but the requirements check callback arrives first and the
  // blacklist result will be discarded.
  ExtensionInstallCheckerAsync checker;
  SetAllErrors(&checker);

  // The policy check is synchronous and needs to pass for the other tests to
  // run.
  checker.set_policy_check_error(std::string());

  RunChecker(&checker,
             true /* fail fast */,
             ExtensionInstallChecker::CHECK_ALL,
             ExtensionInstallChecker::CHECK_ALL,
             ExtensionInstallChecker::CHECK_REQUIREMENTS);

  ExpectRequirementsError(checker);
  ExpectPolicyPass(checker);
  ExpectBlacklistPass(checker);
}

// Test multiple invocations of the install checker. Wait for all checks to
// complete.
TEST_F(ExtensionInstallCheckerMultipleInvocationTest, CompleteAll) {
  ExtensionInstallCheckerAsync checker;
  SetAllErrors(&checker);

  // Start the second check as soon as the callback of the first run is invoked.
  checker.Start(
      ExtensionInstallChecker::CHECK_ALL,
      false /* fail fast */,
      base::Bind(
          &ExtensionInstallCheckerMultipleInvocationTest::RunSecondInvocation,
          base::Unretained(this),
          &checker));
  base::RunLoop().RunUntilIdle();
}

// Test multiple invocations of the install checker and fail fast.
TEST_F(ExtensionInstallCheckerMultipleInvocationTest, FailFast) {
  ExtensionInstallCheckerAsync checker;
  SetAllErrors(&checker);

  // The policy check is synchronous and needs to pass for the other tests to
  // run.
  checker.set_policy_check_error(std::string());

  // Start the second check as soon as the callback of the first run is invoked.
  checker.Start(
      ExtensionInstallChecker::CHECK_ALL,
      true /* fail fast */,
      base::Bind(
          &ExtensionInstallCheckerMultipleInvocationTest::RunSecondInvocation,
          base::Unretained(this),
          &checker));
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
