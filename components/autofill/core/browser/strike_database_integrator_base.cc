// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database_integrator_base.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

namespace {
const char kKeyDeliminator[] = "__";
}  // namespace

StrikeDatabaseIntegratorBase::StrikeDatabaseIntegratorBase(
    StrikeDatabase* strike_database)
    : strike_database_(strike_database) {}

StrikeDatabaseIntegratorBase::~StrikeDatabaseIntegratorBase() {}

bool StrikeDatabaseIntegratorBase::IsMaxStrikesLimitReached(
    const std::string id) {
  CheckIdUniqueness(id);
  return GetStrikes(id) >= GetMaxStrikesLimit();
}

int StrikeDatabaseIntegratorBase::AddStrike(const std::string id) {
  CheckIdUniqueness(id);
  return AddStrikes(1, id);
}

int StrikeDatabaseIntegratorBase::AddStrikes(int strikes_increase,
                                             const std::string id) {
  CheckIdUniqueness(id);
  int num_strikes = strike_database_->AddStrikes(strikes_increase, GetKey(id));
  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.NthStrikeAdded." + GetProjectPrefix(),
      num_strikes);
  return num_strikes;
}

int StrikeDatabaseIntegratorBase::RemoveStrike(const std::string id) {
  CheckIdUniqueness(id);
  return strike_database_->RemoveStrikes(1, GetKey(id));
}

int StrikeDatabaseIntegratorBase::RemoveStrikes(int strikes_decrease,
                                                const std::string id) {
  CheckIdUniqueness(id);
  return strike_database_->RemoveStrikes(strikes_decrease, GetKey(id));
}

int StrikeDatabaseIntegratorBase::GetStrikes(const std::string id) {
  CheckIdUniqueness(id);
  return strike_database_->GetStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::ClearStrikes(const std::string id) {
  CheckIdUniqueness(id);
  strike_database_->ClearStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::RemoveExpiredStrikes() {
  std::vector<std::string> expired_keys;
  for (auto entry : strike_database_->strike_map_cache_) {
    if (AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() -
            entry.second.last_update_timestamp() >
        GetExpiryTimeMicros()) {
      if (strike_database_->GetStrikes(entry.first) > 0)
        expired_keys.push_back(entry.first);
    }
  }
  for (std::string key : expired_keys)
    strike_database_->RemoveStrikes(1, key);
}

std::string StrikeDatabaseIntegratorBase::GetKey(const std::string id) {
  return GetProjectPrefix() + kKeyDeliminator + id;
}

}  // namespace autofill
