// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

sync_pb::PasswordSpecifics SpecificsFromPassword(
    const autofill::PasswordForm& password_form) {
  sync_pb::PasswordSpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_client_only_encrypted_data();
  password_data->set_scheme(password_form.scheme);
  password_data->set_signon_realm(password_form.signon_realm);
  password_data->set_origin(password_form.origin.spec());
  password_data->set_action(password_form.action.spec());
  password_data->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data->set_preferred(password_form.preferred);
  password_data->set_date_created(
      password_form.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data->set_blacklisted(password_form.blacklisted_by_user);
  password_data->set_type(password_form.type);
  password_data->set_times_used(password_form.times_used);
  password_data->set_display_name(
      base::UTF16ToUTF8(password_form.display_name));
  password_data->set_avatar_url(password_form.icon_url.spec());
  password_data->set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
  return specifics;
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const autofill::PasswordForm& form) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_password() = SpecificsFromPassword(form);
  entity_data->non_unique_name = form.signon_realm;
  return entity_data;
}

}  // namespace

PasswordSyncBridge::PasswordSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    PasswordStoreSync* password_store_sync)
    : ModelTypeSyncBridge(std::move(change_processor)),
      password_store_sync_(password_store_sync) {
  DCHECK(password_store_sync_);
}

PasswordSyncBridge::~PasswordSyncBridge() = default;

void PasswordSyncBridge::ActOnPasswordStoreChanges(
    const PasswordStoreChangeList& local_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's the responsibility of the callers to call this method within the same
  // transaction as the data changes to fulfill atomic writes of data and
  // metadata constraint.

  // TODO(mamir):ActOnPasswordStoreChanges() DCHECK we are inside a
  // transaction!;

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // TODO(mamir):ActOnPasswordStoreChanges() can be called from
  // ApplySyncChanges(). Do nothing in this case.

  syncer::SyncMetadataStoreChangeList metadata_change_list(
      password_store_sync_->GetMetadataStore(), syncer::PASSWORDS);

  for (const PasswordStoreChange& change : local_changes) {
    const std::string storage_key = base::NumberToString(change.primary_key());
    switch (change.type()) {
      case PasswordStoreChange::ADD:
      case PasswordStoreChange::UPDATE: {
        change_processor()->Put(storage_key, CreateEntityData(change.form()),
                                &metadata_change_list);
        break;
      }
      case PasswordStoreChange::REMOVE: {
        change_processor()->Delete(storage_key, &metadata_change_list);
        break;
      }
    }
  }
}

void PasswordSyncBridge::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<syncer::MetadataChangeList>
PasswordSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void PasswordSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  NOTIMPLEMENTED();
}

void PasswordSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string PasswordSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_password())
      << "EntityData does not have password specifics.";

  const sync_pb::PasswordSpecificsData& password_data =
      entity_data.specifics.password().client_only_encrypted_data();

  return (net::EscapePath(GURL(password_data.origin()).spec()) + "|" +
          net::EscapePath(password_data.username_element()) + "|" +
          net::EscapePath(password_data.username_value()) + "|" +
          net::EscapePath(password_data.password_element()) + "|" +
          net::EscapePath(password_data.signon_realm()));
}

std::string PasswordSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTREACHED() << "PasswordSyncBridge does not support GetStorageKey.";
  return std::string();
}

bool PasswordSyncBridge::SupportsGetStorageKey() const {
  return false;
}

syncer::ModelTypeSyncBridge::StopSyncResponse
PasswordSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
  return ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync;
}

}  // namespace password_manager
