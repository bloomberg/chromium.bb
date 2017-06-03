// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace arc {

namespace {

std::string GetHistogramName(const std::string& base_name, bool managed) {
  return base_name + (managed ? "Managed" : "Unmanaged");
}

}  // namespace

void UpdateOptInActionUMA(OptInActionType type) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInAction", static_cast<int>(type),
                            static_cast<int>(OptInActionType::SIZE));
}

void UpdateOptInCancelUMA(OptInCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInCancel", static_cast<int>(reason),
                            static_cast<int>(OptInCancelReason::SIZE));
}

void UpdateEnabledStateUMA(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Arc.State", enabled);
}

void UpdateOptInFlowResultUMA(OptInFlowResult result) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInResult", static_cast<int>(result),
                            static_cast<int>(OptInFlowResult::SIZE));
}

void UpdateProvisioningResultUMA(ProvisioningResult result, bool managed) {
  DCHECK_NE(result, ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  base::LinearHistogram::FactoryGet(
      GetHistogramName("Arc.Provisioning.Result.", managed), 0,
      static_cast<int>(ProvisioningResult::SIZE),
      static_cast<int>(ProvisioningResult::SIZE) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(result));
}

void UpdateProvisioningTiming(const base::TimeDelta& elapsed_time,
                              bool success,
                              bool managed) {
  std::string histogram_name = "Arc.Provisioning.TimeDelta.";
  histogram_name += success ? "Success." : "Failure.";
  // The macro UMA_HISTOGRAM_CUSTOM_TIMES expects a constant string, but since
  // this measurement happens very infrequently, we do not need to use a macro
  // here.
  base::UmaHistogramCustomTimes(GetHistogramName(histogram_name, managed),
                                elapsed_time, base::TimeDelta::FromSeconds(1),
                                base::TimeDelta::FromMinutes(6), 50);
}

void UpdatePlayStoreShowTime(const base::TimeDelta& elapsed_time,
                             bool managed) {
  base::UmaHistogramCustomTimes(
      GetHistogramName("Arc.PlayStoreShown.TimeDelta.", managed), elapsed_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromMinutes(10), 50);
}

void UpdateAuthTiming(const char* histogram_name,
                      base::TimeDelta elapsed_time) {
  base::UmaHistogramCustomTimes(histogram_name, elapsed_time,
                                base::TimeDelta::FromSeconds(1) /* minimum */,
                                base::TimeDelta::FromMinutes(3) /* maximum */,
                                50 /* bucket_count */);
}

void UpdateAuthCheckinAttempts(int32_t num_attempts) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("ArcAuth.CheckinAttempts", num_attempts);
}

void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status) {
  DCHECK_LE(status, mojom::AccountCheckStatus::CHECK_FAILED);
  UMA_HISTOGRAM_ENUMERATION(
      "ArcAuth.AccountCheckStatus", static_cast<int>(status),
      static_cast<int>(mojom::AccountCheckStatus::CHECK_FAILED) + 1);
}

void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Arc.OptInSilentAuthCode",
                              static_cast<int>(state));
}

std::ostream& operator<<(std::ostream& os, const ProvisioningResult& result) {
#define MAP_PROVISIONING_RESULT(name) \
  case ProvisioningResult::name:      \
    return os << #name

  switch (result) {
    MAP_PROVISIONING_RESULT(SUCCESS);
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(GMS_NETWORK_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SERVICE_UNAVAILABLE);
    MAP_PROVISIONING_RESULT(GMS_BAD_AUTHENTICATION);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_FAILED);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_FAILED);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(MOJO_CALL_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_FAILED);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_TIMEOUT);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(ARC_STOPPED);
    MAP_PROVISIONING_RESULT(OVERALL_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(CHROME_SERVER_COMMUNICATION_ERROR);
    MAP_PROVISIONING_RESULT(NO_NETWORK_CONNECTION);
    MAP_PROVISIONING_RESULT(ARC_DISABLED);
    MAP_PROVISIONING_RESULT(SIZE);
  }

#undef MAP_PROVISIONING_RESULT

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(result);
  return os;
}

}  // namespace arc
