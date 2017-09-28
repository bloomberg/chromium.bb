// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/address_normalization_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class AddressNormalizationManagerTest : public testing::Test {
 protected:
  AddressNormalizationManagerTest() {}

  void Initialize(const std::string& country_code) {
    address_normalizer_ = base::MakeUnique<autofill::TestAddressNormalizer>();
    manager_ = base::MakeUnique<AddressNormalizationManager>(
        address_normalizer_.get(), country_code);
  }

  void Finalize() {
    manager_->FinalizePendingRequestsWithCompletionCallback(
        base::BindOnce(&AddressNormalizationManagerTest::CompletionCallback,
                       base::Unretained(this)));
  }

  void CompletionCallback() { completion_callback_called_ = true; }

  std::unique_ptr<autofill::TestAddressNormalizer> address_normalizer_;
  std::unique_ptr<AddressNormalizationManager> manager_;
  bool completion_callback_called_ = false;
};

TEST_F(AddressNormalizationManagerTest, SynchronousResult) {
  Initialize("US");

  autofill::AutofillProfile profile_to_normalize;
  manager_->StartNormalizingAddress(&profile_to_normalize);

  EXPECT_FALSE(completion_callback_called_);
  Finalize();
  EXPECT_TRUE(completion_callback_called_);
}

TEST_F(AddressNormalizationManagerTest, AsynchronousResult) {
  Initialize("US");
  address_normalizer_->DelayNormalization();

  autofill::AutofillProfile profile_to_normalize;
  manager_->StartNormalizingAddress(&profile_to_normalize);

  EXPECT_FALSE(completion_callback_called_);
  Finalize();
  EXPECT_FALSE(completion_callback_called_);
  address_normalizer_->CompleteAddressNormalization();
  EXPECT_TRUE(completion_callback_called_);
}

}  // namespace payments
