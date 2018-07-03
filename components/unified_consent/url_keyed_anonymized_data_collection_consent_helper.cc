// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_anonymized_data_collection_consent_helper.h"

#include "base/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

namespace {

class PrefBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedAnonymizedDataCollectionConsentHelper {
 public:
  explicit PrefBasedUrlKeyedDataCollectionConsentHelper(
      PrefService* pref_service);
  ~PrefBasedUrlKeyedDataCollectionConsentHelper() override = default;

  // UrlKeyedAnonymizedDataCollectionConsentHelper:
  bool IsEnabled() override;

 private:
  void OnPrefChanged();
  PrefService* pref_service_;  // weak (must outlive this)
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefBasedUrlKeyedDataCollectionConsentHelper);
};

class SyncBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedAnonymizedDataCollectionConsentHelper,
      syncer::SyncServiceObserver {
 public:
  explicit SyncBasedUrlKeyedDataCollectionConsentHelper(
      syncer::SyncService* sync_service);
  ~SyncBasedUrlKeyedDataCollectionConsentHelper() override;

  // UrlKeyedAnonymizedDataCollectionConsentHelper:
  bool IsEnabled() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  syncer::SyncService* sync_service_;
  syncer::UploadState sync_history_upload_state_;

  DISALLOW_COPY_AND_ASSIGN(SyncBasedUrlKeyedDataCollectionConsentHelper);
};

PrefBasedUrlKeyedDataCollectionConsentHelper::
    PrefBasedUrlKeyedDataCollectionConsentHelper(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(
          &PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged,
          base::Unretained(this)));
}

bool PrefBasedUrlKeyedDataCollectionConsentHelper::IsEnabled() {
  return pref_service_->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

void PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged() {
  FireOnStateChanged();
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    SyncBasedUrlKeyedDataCollectionConsentHelper(
        syncer::SyncService* sync_service)
    : sync_service_(sync_service),
      sync_history_upload_state_(syncer::GetUploadToGoogleState(
          sync_service_,
          syncer::ModelType::HISTORY_DELETE_DIRECTIVES)) {
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    ~SyncBasedUrlKeyedDataCollectionConsentHelper() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

bool SyncBasedUrlKeyedDataCollectionConsentHelper::IsEnabled() {
  return sync_history_upload_state_ == syncer::UploadState::ACTIVE;
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnStateChanged(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  bool enabled_before_state_updated = IsEnabled();
  sync_history_upload_state_ = syncer::GetUploadToGoogleState(
      sync_service_, syncer::ModelType::HISTORY_DELETE_DIRECTIVES);

  if (enabled_before_state_updated != IsEnabled())
    FireOnStateChanged();
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

}  // namespace

UrlKeyedAnonymizedDataCollectionConsentHelper::
    UrlKeyedAnonymizedDataCollectionConsentHelper() = default;
UrlKeyedAnonymizedDataCollectionConsentHelper::
    ~UrlKeyedAnonymizedDataCollectionConsentHelper() = default;

std::unique_ptr<UrlKeyedAnonymizedDataCollectionConsentHelper>
UrlKeyedAnonymizedDataCollectionConsentHelper::NewInstance(
    bool is_unified_consent_enabled,
    PrefService* pref_service,
    syncer::SyncService* sync_service) {
  if (is_unified_consent_enabled) {
    return std::make_unique<PrefBasedUrlKeyedDataCollectionConsentHelper>(
        pref_service);
  }

  return std::make_unique<SyncBasedUrlKeyedDataCollectionConsentHelper>(
      sync_service);
}

void UrlKeyedAnonymizedDataCollectionConsentHelper::AddObserver(
    Observer* observer) {
  observer_list_.AddObserver(observer);
}
void UrlKeyedAnonymizedDataCollectionConsentHelper::RemoveObserver(
    Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UrlKeyedAnonymizedDataCollectionConsentHelper::FireOnStateChanged() {
  bool is_enabled = IsEnabled();
  for (auto& observer : observer_list_)
    observer.OnUrlKeyedDataCollectionConsentStateChanged(is_enabled);
}

}  // namespace unified_consent
