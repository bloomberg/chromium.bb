// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_

#include <vector>

#include "base/macros.h"
#include "components/consent_auditor/consent_auditor.h"

namespace consent_auditor {

class FakeConsentAuditor : public ConsentAuditor {
 public:
  FakeConsentAuditor();
  ~FakeConsentAuditor() override;

  // ConsentAuditor implementation.
  void RecordSyncConsent(
      const std::string& account_id,
      const sync_pb::UserConsentTypes::SyncConsent& consent) override;

  void RecordArcPlayConsent(
      const std::string& account_id,
      const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent& consent)
      override;

  void RecordArcGoogleLocationServiceConsent(
      const std::string& account_id,
      const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent& consent)
      override;

  void RecordArcBackupAndRestoreConsent(
      const std::string& account_id,
      const sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent& consent)
      override;

  void RecordUnifiedConsent(
      const std::string& account_id,
      const sync_pb::UserConsentTypes::UnifiedConsent& consent) override;

  void RecordGaiaConsent(const std::string& account_id,
                         consent_auditor::Feature feature,
                         const std::vector<int>& description_grd_ids,
                         int confirmation_grd_id,
                         consent_auditor::ConsentStatus status) override;
  void RecordLocalConsent(const std::string& feature,
                          const std::string& description_text,
                          const std::string& confirmation_text) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateOnUIThread() override;

  // Methods for fake.
  const std::string& account_id() const { return account_id_; }

  const sync_pb::UserConsentTypes::SyncConsent& recorded_sync_consent() const {
    return recorded_sync_consent_;
  }

  const std::vector<std::vector<int>>& recorded_id_vectors() {
    return recorded_id_vectors_;
  }

  const std::vector<int>& recorded_confirmation_ids() const {
    return recorded_confirmation_ids_;
  }

  const std::vector<Feature>& recorded_features() { return recorded_features_; }

  const std::vector<ConsentStatus>& recorded_statuses() {
    return recorded_statuses_;
  }

  std::string account_id_;

  sync_pb::UserConsentTypes::SyncConsent recorded_sync_consent_;

  std::vector<std::vector<int>> recorded_id_vectors_;
  std::vector<int> recorded_confirmation_ids_;
  std::vector<Feature> recorded_features_;
  std::vector<ConsentStatus> recorded_statuses_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsentAuditor);
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
