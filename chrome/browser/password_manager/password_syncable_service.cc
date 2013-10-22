// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_syncable_service.h"

#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store.h"
#include "components/autofill/core/common/password_form.h"
#include "net/base/escape.h"
#include "sync/api/sync_error_factory.h"

namespace {

// Converts the |PasswordSpecifics| obtained from sync to an
// object of type |PasswordForm|.
void ExtractPasswordFromSpecifics(
     const sync_pb::PasswordSpecificsData& password,
     autofill::PasswordForm* new_password) {
  new_password->scheme =
      static_cast<autofill::PasswordForm::Scheme>(password.scheme());
  new_password->signon_realm = password.signon_realm();
  new_password->origin = GURL(password.origin());
  new_password->action = GURL(password.action());
  new_password->username_element =
      UTF8ToUTF16(password.username_element());
  new_password->password_element =
      UTF8ToUTF16(password.password_element());
  new_password->username_value =
      UTF8ToUTF16(password.username_value());
  new_password->password_value =
      UTF8ToUTF16(password.password_value());
  new_password->ssl_valid = password.ssl_valid();
  new_password->preferred = password.preferred();
  new_password->date_created =
      base::Time::FromInternalValue(password.date_created());
  new_password->blacklisted_by_user =
      password.blacklisted();
}

// Merges the sync password (obtained from the password specifics) and
// local password and stores the output in the |new_password_form| pointer.
bool MergeLocalAndSyncPasswords(
    const sync_pb::PasswordSpecificsData& password_specifics,
    const autofill::PasswordForm& password_form,
    autofill::PasswordForm* new_password_form) {
  if (password_specifics.scheme() == password_form.scheme &&
      password_form.signon_realm == password_specifics.signon_realm() &&
      password_form.origin.spec() == password_specifics.origin() &&
      password_form.action.spec() == password_specifics.action() &&
      UTF16ToUTF8(password_form.username_element) ==
          password_specifics.username_element() &&
      UTF16ToUTF8(password_form.password_element) ==
          password_specifics.password_element() &&
      UTF16ToUTF8(password_form.username_value) ==
          password_specifics.username_value() &&
      UTF16ToUTF8(password_form.password_value) ==
          password_specifics.password_value() &&
      password_specifics.ssl_valid() == password_form.ssl_valid &&
      password_specifics.preferred() == password_form.preferred &&
      password_specifics.date_created() ==
          password_form.date_created.ToInternalValue() &&
      password_specifics.blacklisted() ==
          password_form.blacklisted_by_user) {
    return false;
  }

  // If the passwords differ, take the one that was created more recently.
  if (base::Time::FromInternalValue(password_specifics.date_created()) <=
          password_form.date_created) {
    *new_password_form = password_form;
  } else {
    ExtractPasswordFromSpecifics(password_specifics, new_password_form);
  }

  return true;
}

}  // namespace

PasswordSyncableService::PasswordSyncableService(
    scoped_refptr<PasswordStore> password_store)
    : password_store_(password_store) {
}

PasswordSyncableService::~PasswordSyncableService() {}

syncer::SyncMergeResult
PasswordSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  syncer::SyncMergeResult merge_result(type);
  sync_error_factory_ = sync_error_factory.Pass();
  sync_processor_ = sync_processor.Pass();

  merge_result.set_error(sync_error_factory->CreateAndUploadError(
      FROM_HERE,
      "Password Syncable Service Not Implemented."));
  return merge_result;
}

void PasswordSyncableService::StopSyncing(syncer::ModelType type) {
}

syncer::SyncDataList PasswordSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  return sync_data;
}

syncer::SyncError PasswordSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Password Syncable Service Not Implemented.",
                          syncer::PASSWORDS);
  return error;
}

void PasswordSyncableService::WriteToPasswordStore(
    PasswordForms* new_entries,
    PasswordForms* updated_entries) {
  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           new_entries->begin();
       it != new_entries->end();
       ++it) {
    password_store_->AddLoginImpl(**it);
  }

  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           updated_entries->begin();
       it != updated_entries->end();
       ++it) {
    password_store_->UpdateLoginImpl(**it);
  }

  if (!new_entries->empty() || !updated_entries->empty()) {
    // We have to notify password store observers of the change by hand since
    // we use internal password store interfaces to make changes synchronously.
    password_store_->PostNotifyLoginsChanged();
  }
}

syncer::SyncData PasswordSyncableService::CreateSyncData(
    const autofill::PasswordForm& password_form) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
  password_specifics->set_scheme(password_form.scheme);
  password_specifics->set_signon_realm(password_form.signon_realm);
  password_specifics->set_origin(password_form.origin.spec());
  password_specifics->set_action(password_form.action.spec());
  password_specifics->set_username_element(
      UTF16ToUTF8(password_form.username_element));
  password_specifics->set_password_element(
      UTF16ToUTF8(password_form.password_element));
  password_specifics->set_username_value(
      UTF16ToUTF8(password_form.username_value));
  password_specifics->set_password_value(
      UTF16ToUTF8(password_form.password_value));
  password_specifics->set_ssl_valid(password_form.ssl_valid);
  password_specifics->set_preferred(password_form.preferred);
  password_specifics->set_date_created(
      password_form.date_created.ToInternalValue());
  password_specifics->set_blacklisted(password_form.blacklisted_by_user);

  std::string tag = MakeTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

// static
std::string PasswordSyncableService::MakeTag(
    const std::string& origin_url,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm) {
  return net::EscapePath(origin_url) + "|" +
         net::EscapePath(username_element) + "|" +
         net::EscapePath(username_value) + "|" +
         net::EscapePath(password_element) + "|" +
         net::EscapePath(signon_realm);
}

// static
std::string PasswordSyncableService::MakeTag(
    const autofill::PasswordForm& password) {
  return MakeTag(password.origin.spec(),
                 UTF16ToUTF8(password.username_element),
                 UTF16ToUTF8(password.username_value),
                 UTF16ToUTF8(password.password_element),
                 password.signon_realm);
}

// static
std::string PasswordSyncableService::MakeTag(
    const sync_pb::PasswordSpecificsData& password) {
  return MakeTag(password.origin(),
                 password.username_element(),
                 password.username_value(),
                 password.password_element(),
                 password.signon_realm());
}

