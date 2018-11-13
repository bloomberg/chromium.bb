// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
class AutofillProfile;
class AutofillWebDataService;
class CreditCard;
class PersonalDataManager;
struct PaymentsCustomerData;
}  // namespace autofill

namespace sync_pb {
class SyncEntity;
}

namespace wallet_helper {

extern const char kDefaultCardID[];
extern const char kDefaultAddressID[];
extern const char kDefaultCustomerID[];
extern const char kDefaultBillingAddressID[];

// Used to access the personal data manager within a particular sync profile.
autofill::PersonalDataManager* GetPersonalDataManager(int index)
    WARN_UNUSED_RESULT;

// Used to access the web data service within a particular sync profile.
scoped_refptr<autofill::AutofillWebDataService> GetProfileWebDataService(
    int index) WARN_UNUSED_RESULT;

// Used to access the account-scoped web data service within a particular sync
// profile.
scoped_refptr<autofill::AutofillWebDataService> GetAccountWebDataService(
    int index) WARN_UNUSED_RESULT;

void SetServerCreditCards(
    int profile,
    const std::vector<autofill::CreditCard>& credit_cards);

void SetServerProfiles(int profile,
                       const std::vector<autofill::AutofillProfile>& profiles);

void SetPaymentsCustomerData(
    int profile,
    const autofill::PaymentsCustomerData& customer_data);

void UpdateServerCardMetadata(int profile,
                              const autofill::CreditCard& credit_card);

void UpdateServerAddressMetadata(
    int profile,
    const autofill::AutofillProfile& server_address);

sync_pb::SyncEntity CreateDefaultSyncWalletCard();

sync_pb::SyncEntity CreateSyncWalletCard(const std::string& name,
                                         const std::string& last_four);

sync_pb::SyncEntity CreateSyncPaymentsCustomerData(
    const std::string& customer_id);

sync_pb::SyncEntity CreateDefaultSyncPaymentsCustomerData();

autofill::CreditCard GetDefaultCreditCard();

autofill::CreditCard GetCreditCard(const std::string& name,
                                   const std::string& last_four);

sync_pb::SyncEntity CreateDefaultSyncWalletAddress();

sync_pb::SyncEntity CreateSyncWalletAddress(const std::string& name,
                                            const std::string& company);

// TODO(sebsg): Instead add a function to create a card, and one to inject in
// the server. Then compare the cards directly.
void ExpectDefaultCreditCardValues(const autofill::CreditCard& card);

// TODO(sebsg): Instead add a function to create a profile, and one to inject in
// the server. Then compare the profiles directly.
void ExpectDefaultProfileValues(const autofill::AutofillProfile& profile);

// Load current data from the database of profile |profile|.
std::vector<autofill::AutofillProfile*> GetServerProfiles(int profile);
std::vector<autofill::CreditCard*> GetServerCreditCards(int profile);

}  // namespace wallet_helper

// Checker to block until autofill wallet & server profiles match on both
// profiles.
class AutofillWalletChecker : public StatusChangeChecker,
                              public autofill::PersonalDataManagerObserver {
 public:
  AutofillWalletChecker(int profile_a, int profile_b);
  ~AutofillWalletChecker() override;

  // StatusChangeChecker implementation.
  bool Wait() override;
  bool IsExitConditionSatisfied() override;
  std::string GetDebugMessage() const override;

  // autofill::PersonalDataManager implementation.
  void OnPersonalDataChanged() override;

 private:
  const int profile_a_;
  const int profile_b_;
};

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
// TODO(jkrcal): When the new implementation fully launches, remove this class,
// convert all tests from *_P back to *_F and remove the instance at the end.
class UssWalletSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssWalletSwitchToggler();

  // Sets up feature overrides, based on the parameter of the test.
  void InitWithDefaultFeatures();

  // Sets up feature overrides, adds the toggled feature on top of specified
  // |enabled_features| and |disabled_features|. Vectors are passed by value
  // because we need to alter them anyway.
  void InitWithFeatures(std::vector<base::Feature> enabled_features,
                        std::vector<base::Feature> disabled_features);

 private:
  base::test::ScopedFeatureList override_features_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_
