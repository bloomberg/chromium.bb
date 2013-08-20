// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_utility.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/managed_mode/managed_user_refresh_token_fetcher.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

using base::DictionaryValue;

const char kAcknowledged[] = "acknowledged";
const char kName[] = "name";
const char kMasterKey[] = "masterKey";

ManagedUserRegistrationInfo::ManagedUserRegistrationInfo(const string16& name)
    : name(name) {
}

ManagedUserRegistrationUtility::ManagedUserRegistrationUtility(
    PrefService* prefs,
    scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
    ManagedUserSyncService* service)
    : weak_ptr_factory_(this),
      prefs_(prefs),
      token_fetcher_(token_fetcher.Pass()),
      managed_user_sync_service_(service),
      pending_managed_user_acknowledged_(false),
      is_existing_managed_user_(false) {
  managed_user_sync_service_->AddObserver(this);
}

ManagedUserRegistrationUtility::~ManagedUserRegistrationUtility() {
  managed_user_sync_service_->RemoveObserver(this);
  CancelPendingRegistration();
}

// static
scoped_ptr<ManagedUserRegistrationUtility>
ManagedUserRegistrationUtility::Create(Profile* profile) {
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher =
      ManagedUserRefreshTokenFetcher::Create(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          profile->GetRequestContext());
  ManagedUserSyncService* managed_user_sync_service =
      ManagedUserSyncServiceFactory::GetForProfile(profile);
  return make_scoped_ptr(new ManagedUserRegistrationUtility(
      profile->GetPrefs(), token_fetcher.Pass(), managed_user_sync_service));
}

// static
std::string ManagedUserRegistrationUtility::GenerateNewManagedUserId() {
  std::string new_managed_user_id;
  bool success = base::Base64Encode(base::RandBytesAsString(8),
                                    &new_managed_user_id);
  DCHECK(success);
  return new_managed_user_id;
}

void ManagedUserRegistrationUtility::Register(
    const std::string& managed_user_id,
    const ManagedUserRegistrationInfo& info,
    const RegistrationCallback& callback) {
  DCHECK(pending_managed_user_id_.empty());
  callback_ = callback;
  pending_managed_user_id_ = managed_user_id;

  const DictionaryValue* dict = prefs_->GetDictionary(prefs::kManagedUsers);
  is_existing_managed_user_ = dict->HasKey(managed_user_id);
  if (!is_existing_managed_user_) {
    managed_user_sync_service_->AddManagedUser(pending_managed_user_id_,
                                               base::UTF16ToUTF8(info.name),
                                               info.master_key);
  } else {
    // User already exists, don't wait for acknowledgment.
    OnManagedUserAcknowledged(managed_user_id);
  }

  browser_sync::DeviceInfo::GetClientName(
      base::Bind(&ManagedUserRegistrationUtility::FetchToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserRegistrationUtility::CancelPendingRegistration() {
  AbortPendingRegistration(
      false,  // Don't run the callback. The error will be ignored.
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void ManagedUserRegistrationUtility::OnManagedUserAcknowledged(
    const std::string& managed_user_id) {
  DCHECK_EQ(pending_managed_user_id_, managed_user_id);
  DCHECK(!pending_managed_user_acknowledged_);
  pending_managed_user_acknowledged_ = true;
  CompleteRegistrationIfReady();
}

void ManagedUserRegistrationUtility::OnManagedUsersSyncingStopped() {
  AbortPendingRegistration(
      true,  // Run the callback.
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

void ManagedUserRegistrationUtility::FetchToken(
    const std::string& client_name) {
  token_fetcher_->Start(
      pending_managed_user_id_, client_name,
      base::Bind(&ManagedUserRegistrationUtility::OnReceivedToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserRegistrationUtility::OnReceivedToken(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    CompleteRegistration(true, error);
    return;
  }

  DCHECK(!token.empty());
  pending_managed_user_token_ = token;
  CompleteRegistrationIfReady();
}

void ManagedUserRegistrationUtility::CompleteRegistrationIfReady() {
  bool require_acknowledgment =
      !pending_managed_user_acknowledged_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoManagedUserAcknowledgmentCheck);
  if (require_acknowledgment || pending_managed_user_token_.empty())
    return;

  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  CompleteRegistration(true, error);
}

void ManagedUserRegistrationUtility::AbortPendingRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  pending_managed_user_token_.clear();
  CompleteRegistration(run_callback, error);
}

void ManagedUserRegistrationUtility::CompleteRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  if (callback_.is_null())
    return;

  // We check that the user being registered is not an existing managed
  // user before deleting it from sync to avoid accidental deletion of
  // existing managed users by just canceling the registration for example.
  if (pending_managed_user_token_.empty() && !is_existing_managed_user_) {
    DCHECK(!pending_managed_user_id_.empty());
    // Remove the pending managed user if we weren't successful.
    DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
    bool success =
        update->RemoveWithoutPathExpansion(pending_managed_user_id_, NULL);
    DCHECK(success);
    managed_user_sync_service_->DeleteManagedUser(pending_managed_user_id_);
  }

  if (run_callback)
    callback_.Run(error, pending_managed_user_token_);
  callback_.Reset();
}
