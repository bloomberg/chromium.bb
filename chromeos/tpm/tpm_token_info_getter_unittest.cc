// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The struct holding information returned by TPMTokenInfoGetter::Start
// callback.
struct TestTPMTokenInfo {
  TestTPMTokenInfo() : enabled(false), slot_id(-2) {}

  bool IsReady() const {
    return slot_id >= 0;
  }

  bool enabled;
  std::string name;
  std::string pin;
  int slot_id;
};

// Callback for TPMTokenInfoGetter::Start. It records the values returned
// by TPMTokenInfoGetter to |info|.
void RecordGetterResult(TestTPMTokenInfo* target,
                        const chromeos::TPMTokenInfo& source) {
  target->enabled = source.tpm_is_enabled;
  target->name = source.token_name;
  target->pin = source.user_pin;
  target->slot_id = source.token_slot_id;
}

// Task runner for handling delayed tasks posted by TPMTokenInfoGetter when
// retrying failed cryptohome method calls. It just records the requested
// delay and immediately runs the task. The task is run asynchronously to be
// closer to what's actually happening in production.
// The delays used by TPMTokenGetter should be monotonically increasing, so
// the fake task runner does not handle task reordering based on the delays.
class FakeTaskRunner : public base::TaskRunner {
 public:
  // |delays|: Vector to which the dalays seen by the task runner are saved.
  explicit FakeTaskRunner(std::vector<int64_t>* delays) : delays_(delays) {}

  // base::TaskRunner overrides:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    delays_->push_back(delay.InMilliseconds());
    base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, std::move(task));
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}

 private:
  // The vector of delays.
  std::vector<int64_t>* delays_;

  DISALLOW_COPY_AND_ASSIGN(FakeTaskRunner);
};

// Implementation of CryptohomeClient used in these tests. Note that
// TestCryptohomeClient implements FakeCryptohomeClient purely for convenience
// of not having to implement whole CryptohomeClient interface.
// TestCryptohomeClient overrides all CryptohomeClient methods used in
// TPMTokenInfoGetter tests.
class TestCryptohomeClient : public chromeos::FakeCryptohomeClient {
 public:
  // |account_id|: The user associated with the TPMTokenInfoGetter that will be
  // using the TestCryptohomeClient. Should be empty for system token.
  explicit TestCryptohomeClient(const AccountId& account_id)
      : account_id_(account_id),
        tpm_is_enabled_(true),
        tpm_is_enabled_failure_count_(0),
        tpm_is_enabled_succeeded_(false),
        get_tpm_token_info_failure_count_(0),
        get_tpm_token_info_not_set_count_(0),
        get_tpm_token_info_succeeded_(false) {}

  ~TestCryptohomeClient() override {}

  void set_tpm_is_enabled(bool value) {
    tpm_is_enabled_ = value;
  }

  void set_tpm_is_enabled_failure_count(int value) {
    ASSERT_GT(value, 0);
    tpm_is_enabled_failure_count_ = value;
  }

  void set_get_tpm_token_info_failure_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_failure_count_ = value;
  }

  void set_get_tpm_token_info_not_set_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_not_set_count_ = value;
  }

  // Sets the tpm tpken info to be reported by the test CryptohomeClient.
  // If there is |Pkcs11GetTpmTokenInfo| in progress, runs the pending
  // callback with the set tpm token info.
  void SetTPMTokenInfo(const std::string& token_name,
                       const std::string& pin,
                       int slot_id) {
    tpm_token_info_.name = token_name;
    tpm_token_info_.pin = pin;
    tpm_token_info_.slot_id = slot_id;

    ASSERT_TRUE(tpm_token_info_.IsReady());

    InvokeGetTpmTokenInfoCallbackIfReady();
  }

 private:
  // FakeCryptohomeClient override.
  void TpmIsEnabled(const chromeos::BoolDBusMethodCallback& callback) override {
    ASSERT_FALSE(tpm_is_enabled_succeeded_);
    if (tpm_is_enabled_failure_count_ > 0) {
      --tpm_is_enabled_failure_count_;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback, chromeos::DBUS_METHOD_CALL_FAILURE, false));
    } else {
      tpm_is_enabled_succeeded_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     chromeos::DBUS_METHOD_CALL_SUCCESS, tpm_is_enabled_));
    }
  }

  void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) override {
    ASSERT_TRUE(account_id_.empty());

    HandleGetTpmTokenInfo(callback);
  }

  void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::Identification& cryptohome_id,
      const Pkcs11GetTpmTokenInfoCallback& callback) override {
    ASSERT_FALSE(cryptohome_id.id().empty());
    ASSERT_EQ(account_id_, cryptohome_id.GetAccountId());

    HandleGetTpmTokenInfo(callback);
  }

  // Handles Pkcs11GetTpmTokenInfo calls (both for system and user token). The
  // CryptohomeClient method overrides should make sure that |account_id_| is
  // properly set before calling this.
  void HandleGetTpmTokenInfo(const Pkcs11GetTpmTokenInfoCallback& callback) {
    ASSERT_TRUE(tpm_is_enabled_succeeded_);
    ASSERT_FALSE(get_tpm_token_info_succeeded_);
    ASSERT_TRUE(pending_get_tpm_token_info_callback_.is_null());

    if (get_tpm_token_info_failure_count_ > 0) {
      --get_tpm_token_info_failure_count_;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     chromeos::DBUS_METHOD_CALL_FAILURE,
                     std::string() /* token name */,
                     std::string() /* user pin */,
                     -1 /* slot id */));
      return;
    }

    if (get_tpm_token_info_not_set_count_ > 0) {
      --get_tpm_token_info_not_set_count_;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     chromeos::DBUS_METHOD_CALL_SUCCESS,
                     std::string() /* token name */,
                     std::string() /* user pin */,
                     -1 /* slot id */));
      return;
    }

    pending_get_tpm_token_info_callback_ = callback;
    InvokeGetTpmTokenInfoCallbackIfReady();
  }

  void InvokeGetTpmTokenInfoCallbackIfReady() {
    if (!tpm_token_info_.IsReady() ||
        pending_get_tpm_token_info_callback_.is_null())
      return;

    get_tpm_token_info_succeeded_ = true;
    // Called synchronously for convenience (to avoid using extra RunLoop in
    // tests). Unlike with other Cryptohome callbacks, TPMTokenInfoGetter does
    // not rely on this callback being called asynchronously.
    pending_get_tpm_token_info_callback_.Run(
        chromeos::DBUS_METHOD_CALL_SUCCESS,
        tpm_token_info_.name,
        tpm_token_info_.pin,
        tpm_token_info_.slot_id);
  }

  AccountId account_id_;
  bool tpm_is_enabled_;
  int tpm_is_enabled_failure_count_;
  bool tpm_is_enabled_succeeded_;
  int get_tpm_token_info_failure_count_;
  int get_tpm_token_info_not_set_count_;
  bool get_tpm_token_info_succeeded_;
  Pkcs11GetTpmTokenInfoCallback pending_get_tpm_token_info_callback_;
  TestTPMTokenInfo tpm_token_info_;

  DISALLOW_COPY_AND_ASSIGN(TestCryptohomeClient);
};

class SystemTPMTokenInfoGetterTest : public testing::Test {
 public:
  SystemTPMTokenInfoGetterTest() {}
  ~SystemTPMTokenInfoGetterTest() override {}

  void SetUp() override {
    cryptohome_client_.reset(new TestCryptohomeClient(EmptyAccountId()));
    tpm_token_info_getter_ =
        chromeos::TPMTokenInfoGetter::CreateForSystemToken(
            cryptohome_client_.get(),
            scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
  }

 protected:
  std::unique_ptr<TestCryptohomeClient> cryptohome_client_;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> tpm_token_info_getter_;

  std::vector<int64_t> delays_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SystemTPMTokenInfoGetterTest);
};

class UserTPMTokenInfoGetterTest : public testing::Test {
 public:
  UserTPMTokenInfoGetterTest()
      : account_id_(AccountId::FromUserEmail("user@gmail.com")) {}
  ~UserTPMTokenInfoGetterTest() override {}

  void SetUp() override {
    cryptohome_client_.reset(new TestCryptohomeClient(account_id_));
    tpm_token_info_getter_ = chromeos::TPMTokenInfoGetter::CreateForUserToken(
        account_id_, cryptohome_client_.get(),
        scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
  }

 protected:
  std::unique_ptr<TestCryptohomeClient> cryptohome_client_;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> tpm_token_info_getter_;

  const AccountId account_id_;
  std::vector<int64_t> delays_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(UserTPMTokenInfoGetterTest);
};

TEST_F(SystemTPMTokenInfoGetterTest, BasicFlow) {
  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TokenSlotIdEqualsZero) {
  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_0", "2222", 0);

  EXPECT_TRUE(reported_info.IsReady());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_0", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(0, reported_info.slot_id);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TPMNotEnabled) {
  cryptohome_client_->set_tpm_is_enabled(false);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  EXPECT_FALSE(reported_info.enabled);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TpmEnabledCallFails) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(1);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelaysIncreaseExponentially) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(2);
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(3);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 2);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(2, reported_info.slot_id);

  int64_t kExpectedDelays[] = {100, 200, 400, 800, 1600, 3200};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelayBounded) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(4);
  cryptohome_client_->set_get_tpm_token_info_failure_count(5);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(6);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  int64_t kExpectedDelays[] = {100,    200,    400,    800,    1600,
                               3200,   6400,   12800,  25600,  51200,
                               102400, 204800, 300000, 300000, 300000};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, BasicFlow) {
  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  TestTPMTokenInfo reported_info;
  tpm_token_info_getter_->Start(
      base::Bind(&RecordGetterResult, &reported_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(reported_info.IsReady());
  cryptohome_client_->SetTPMTokenInfo("TOKEN_1", "2222", 1);

  EXPECT_TRUE(reported_info.IsReady());
  EXPECT_TRUE(reported_info.enabled);
  EXPECT_EQ("TOKEN_1", reported_info.name);
  EXPECT_EQ("2222", reported_info.pin);
  EXPECT_EQ(1, reported_info.slot_id);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + arraysize(kExpectedDelays)),
            delays_);
}

}  // namespace
