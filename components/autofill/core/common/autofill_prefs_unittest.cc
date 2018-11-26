// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include <memory>

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace prefs {

class AutofillPrefsTest : public testing::Test {
 protected:
  AutofillPrefsTest() {}
  ~AutofillPrefsTest() override {}

  void SetUp() override { pref_service_ = CreatePrefServiceAndRegisterPrefs(); }

  // Creates a PrefService and registers Autofill prefs.
  std::unique_ptr<PrefService> CreatePrefServiceAndRegisterPrefs() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable());
    RegisterProfilePrefs(registry.get());
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
    return factory.Create(registry);
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<PrefService> pref_service_;
};

// Tests migrating the value of the deprecated Autofill master pref to the new
// prefs and that this operation takes place only once.
TEST_F(AutofillPrefsTest, MigrateDeprecatedAutofillPrefs) {
  // All prefs should be enabled by default.
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Disable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, false);
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should become the same as the deprecated master pref's value.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Enable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, true);
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should not be affected when migration happens a second time.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));
}

// Tests that setting and getting the AutofillSyncTransportOptIn works as
// expected.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_GetAndSet) {
  const std::string account1 = "account1";
  const std::string account2 = "account2";

  // There should be no opt-in recorded at first.
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  auto* upload_events =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account1, base::Value::Type::INTEGER));
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account2, base::Value::Type::INTEGER));

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be an entry for the first account only in the dictionary.
  upload_events =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  EXPECT_NE(nullptr,
            upload_events->FindKeyOfType(account1, base::Value::Type::INTEGER));
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account2, base::Value::Type::INTEGER));

  // Unset the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, false);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  upload_events =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account1, base::Value::Type::INTEGER));
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account2, base::Value::Type::INTEGER));

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be an entry for the second account only in the dictionary.
  upload_events =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(nullptr,
            upload_events->FindKeyOfType(account1, base::Value::Type::INTEGER));
  EXPECT_NE(nullptr,
            upload_events->FindKeyOfType(account2, base::Value::Type::INTEGER));
}

// Tests that clearing the AutofillSyncTransportOptIn works as expected.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_Clear) {
  const std::string account1 = "account1";
  const std::string account2 = "account2";

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  // Clear all opt-ins. The dictionary should be empty.
  ClearSyncTransportOptIns(pref_service());
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());
}

}  // namespace prefs
}  // namespace autofill