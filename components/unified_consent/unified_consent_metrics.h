// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_

namespace unified_consent {

namespace metrics {

// Histogram enum: UnifiedConsentBumpAction.
enum class UnifiedConsentBumpAction : int {
  kUnifiedConsentBumpActionDefaultOptIn = 0,
  kUnifiedConsentBumpActionMoreOptionsOptIn,
  kUnifiedConsentBumpActionMoreOptionsReviewSettings,
  kUnifiedConsentBumpActionMoreOptionsNoChanges,
  kUnifiedConsentBumpActionMoreOptionsMax,
};

// Histogram enum: UnifiedConsentRevokeReason.
enum class UnifiedConsentRevokeReason : int {
  kUserSignedOut = 0,
  kServiceWasDisabled,
  kCustomPassphrase,
  kMaxValue = kCustomPassphrase
};

// Records histogram action for the unified consent bump.
void RecordConsentBumpMetric(UnifiedConsentBumpAction action);

// Records whether the user is eligible for the consent bump. This method should
// be called at startup.
void RecordConsentBumpEligibility(bool eligible);

// Records the reason why the unified consent was revoked.
void RecordUnifiedConsentRevoked(UnifiedConsentRevokeReason reason);

}  // namespace metrics

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
