// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cryptohome_client.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/attestation/attestation.pb.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace chromeos {

class FakeCryptohomeClientTest : public ::testing::Test {
 public:
  FakeCryptohomeClientTest() : weak_ptr_factory_(this) {
    fake_cryptohome_client_.SetAsyncCallStatusHandlers(
        base::Bind(
            &FakeCryptohomeClientTest::MockHandleAsyncMethodResultResponse,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&FakeCryptohomeClientTest::MockHandleAsyncMethodDataResponse,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  MOCK_METHOD3(MockHandleAsyncMethodResultResponse, void(int, bool, int));
  MOCK_METHOD3(MockHandleAsyncMethodDataResponse,
               void(int, bool, const std::string&));

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  FakeCryptohomeClient fake_cryptohome_client_;

  base::WeakPtrFactory<FakeCryptohomeClientTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClientTest);
};

TEST_F(FakeCryptohomeClientTest, SignSimpleChallenge) {
  const std::string challenge{"challenge"};

  std::string return_data;
  EXPECT_CALL(*this, MockHandleAsyncMethodDataResponse(_, true, _))
      .WillOnce(SaveArg<2>(&return_data));

  cryptohome::Identification cryptohome_id;
  bool called = false;
  fake_cryptohome_client_.TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType::KEY_DEVICE, cryptohome_id, "key_name",
      challenge,
      base::BindOnce(
          [](bool* called, base::Optional<int> async_id) { *called = true; },
          &called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  chromeos::attestation::SignedData signed_data;
  ASSERT_TRUE(signed_data.ParseFromString(return_data));
  ASSERT_EQ(static_cast<size_t>(20),
            signed_data.data().size() - challenge.size());
  ASSERT_EQ(challenge,
            signed_data.data().substr(0, signed_data.data().size() - 20));
}

}  // namespace chromeos
