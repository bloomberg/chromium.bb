// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_BASE_H_

#include "components/autofill/core/browser/strike_database.h"

namespace autofill {

// Contains virtual functions for per-project implementations of StrikeDatabase
// to interface from, as well as a pointer to StrikeDatabase. This class is
// seperated from StrikeDatabase since we only want StrikeDatabase's cache to
// be loaded once per browser session.
class StrikeDatabaseIntegratorBase {
 public:
  StrikeDatabaseIntegratorBase(StrikeDatabase* strike_database);
  virtual ~StrikeDatabaseIntegratorBase();

  // Returns whether or not strike count for |id| has reached the strike limit
  // set by GetMaxStrikesLimit().
  bool IsMaxStrikesLimitReached(const std::string id);

  // Increments in-memory cache and updates underlying ProtoDatabase.
  int AddStrike(const std::string id);

  // Removes an in-memory cache strike, updates last_update_timestamp, and
  // updates underlying ProtoDatabase.
  int RemoveStrike(const std::string id);

  // Returns strike count from in-memory cache.
  int GetStrikes(const std::string id);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase.
  void ClearStrikes(const std::string id);

 protected:
  // Removes all strikes in which it has been longer than GetExpiryTimeMicros()
  // past |last_update_timestamp|.
  void RemoveExpiredStrikes();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetKeyForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetIdForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           RemoveExpiredStrikesTest);
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  StrikeDatabase* strike_database_;

  // Generates key based on project-specific string identifier.
  std::string GetKey(const std::string id);

  // Returns a prefix unique to each project, which will be used to create
  // database key.
  virtual std::string GetProjectPrefix() = 0;

  // Returns the maximum number of strikes after which the project's Autofill
  // opportunity stops being offered.
  virtual int GetMaxStrikesLimit() = 0;

  // Returns the time after which the most recent strike should expire.
  virtual long long GetExpiryTimeMicros() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_BASE_H_
