// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_constants.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcAuthService* arc_auth_service = nullptr;

const char kArcSupportExtensionId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";
const char kArcSupportStorageId[] = "arc_support";

// Skip creating UI in unit tests
bool disable_ui_for_testing = false;

const char kStateStopped[] = "STOPPED";
const char kStateFetchingCode[] = "FETCHING_CODE";
const char kStateActive[] = "ACTIVE";
}  // namespace

ArcAuthService::ArcAuthService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  DCHECK(!arc_auth_service);
  arc_auth_service = this;

  arc_bridge_service()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  DCHECK(!profile_);
  arc_bridge_service()->RemoveObserver(this);

  DCHECK(arc_auth_service == this);
  arc_auth_service = nullptr;
}

// static
ArcAuthService* ArcAuthService::Get() {
  DCHECK(arc_auth_service);
  DCHECK(arc_auth_service->thread_checker_.CalledOnValidThread());
  return arc_auth_service;
}

// static
void ArcAuthService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kArcEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kArcSignedIn, false);
}

// static
void ArcAuthService::DisableUIForTesting() {
  disable_ui_for_testing = true;
}

// static
bool ArcAuthService::IsOptInVerificationDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

void ArcAuthService::OnAuthInstanceReady() {
  arc_bridge_service()->auth_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

std::string ArcAuthService::GetAndResetAuthCode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string auth_code;
  auth_code_.swap(auth_code);
  return auth_code;
}

void ArcAuthService::GetAuthCodeDeprecated(
    const GetAuthCodeDeprecatedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!IsOptInVerificationDisabled());
  callback.Run(mojo::String(GetAndResetAuthCode()));
}

void ArcAuthService::GetAuthCode(const GetAuthCodeCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string auth_code = GetAndResetAuthCode();
  if (!auth_code.empty()) {
    callback.Run(mojo::String(auth_code), !IsOptInVerificationDisabled());
    return;
  }

  auth_callback_ = callback;
  SetState(State::FETCHING_CODE);
  FetchAuthCode();
}

void ArcAuthService::OnSignInComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::ACTIVE);

  profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
}

void ArcAuthService::OnSignInFailed(arc::ArcSignInFailureReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::ACTIVE);

  profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
  ShutdownBridgeAndCloseUI();
}

void ArcAuthService::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInChanged(state_));
}

void ArcAuthService::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK(profile && profile != profile_);
  DCHECK(thread_checker_.CalledOnValidThread());

  Shutdown();

  if (profile->IsLegacySupervised()) {
    VLOG(2) << "Supervised profiles are not supported in Arc.";
    return;
  }

  profile_ = profile;
  // Reuse storage used in ARC OptIn platform app.
  const std::string site_url =
      base::StringPrintf("%s://%s/persist?%s", content::kGuestScheme,
                         kArcSupportExtensionId, kArcSupportStorageId);
  storage_partition_ = content::BrowserContext::GetStoragePartitionForSite(
      profile_, GURL(site_url));
  CHECK(storage_partition_);

  // In case UI is disabled we assume that ARC is opted-in.
  if (!IsOptInVerificationDisabled()) {
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kArcEnabled,
        base::Bind(&ArcAuthService::OnOptInPreferenceChanged,
                   base::Unretained(this)));
    if (profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
      OnOptInPreferenceChanged();
    } else {
      if (!disable_ui_for_testing && profile_->IsNewProfile()) {
        PrefServiceSyncableFromProfile(profile_)->AddObserver(this);
        OnIsSyncingChanged();
      }
    }
  } else {
    auth_code_.clear();
    StartArc();
  }
}

void ArcAuthService::OnIsSyncingChanged() {
  syncable_prefs::PrefServiceSyncable* const pref_service_syncable =
      PrefServiceSyncableFromProfile(profile_);
  if (!pref_service_syncable->IsSyncing())
    return;

  pref_service_syncable->RemoveObserver(this);
  if (!profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled))
    arc::ArcAuthNotification::Show();
}

void ArcAuthService::Shutdown() {
  ShutdownBridgeAndCloseUI();
  if (profile_)
    PrefServiceSyncableFromProfile(profile_)->RemoveObserver(this);
  profile_ = nullptr;
  pref_change_registrar_.RemoveAll();
}

void ArcAuthService::OnMergeSessionSuccess(const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          kArcSupportExtensionId);
  CHECK(extension &&
        extensions::util::IsAppLaunchable(kArcSupportExtensionId, profile_));

  AppLaunchParams params(profile_, extension, NEW_WINDOW,
                         extensions::SOURCE_CHROME_INTERNAL);
  OpenApplication(params);
}

void ArcAuthService::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "Failed to merge gaia session " << error.ToString() << ".";
  OnAuthCodeFailed();
}

void ArcAuthService::OnUbertokenSuccess(const std::string& token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  merger_fetcher_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeOSSource,
                          storage_partition_->GetURLRequestContext()));
  merger_fetcher_->StartMergeSession(token, std::string());
}

void ArcAuthService::OnUbertokenFailure(const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "Failed to get ubertoken " << error.ToString() << ".";
  OnAuthCodeFailed();
}

void ArcAuthService::OnOptInPreferenceChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(profile_);

  if (profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    if (state_ != State::ACTIVE) {
      CloseUI();
      auth_code_.clear();

      if (!profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn)) {
        // Need pre-fetch auth code and show OptIn UI if needed.
        SetState(State::FETCHING_CODE);
        FetchAuthCode();
      } else {
        // Ready to start Arc.
        StartArc();
      }
    }
  } else {
    ShutdownBridgeAndCloseUI();
  }
}

void ArcAuthService::ShutdownBridgeAndCloseUI() {
  CloseUI();
  auth_callback_.reset();
  auth_fetcher_.reset();
  ubertoken_fethcher_.reset();
  merger_fetcher_.reset();
  ArcBridgeService::Get()->Shutdown();
  SetState(State::STOPPED);
}

void ArcAuthService::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void ArcAuthService::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcAuthService::CloseUI() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInUINeedToClose());
}

void ArcAuthService::StartArc() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ArcBridgeService::Get()->HandleStartup();
  SetState(State::ACTIVE);
}

void ArcAuthService::SetAuthCodeAndStartArc(const std::string& auth_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!auth_code.empty());

  if (!auth_callback_.is_null()) {
    DCHECK_EQ(state_, State::FETCHING_CODE);
    SetState(State::ACTIVE);
    CloseUI();
    auth_callback_.Run(mojo::String(auth_code), !IsOptInVerificationDisabled());
    auth_callback_.reset();
    return;
  }

  State state = state_;

  ShutdownBridgeAndCloseUI();

  if (state != State::FETCHING_CODE)
    return;

  auth_code_ = auth_code;
  StartArc();
}

void ArcAuthService::FetchAuthCode() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != State::FETCHING_CODE)
    return;

  auth_fetcher_.reset(
      new ArcAuthFetcher(storage_partition_->GetURLRequestContext(), this));
}

void ArcAuthService::CancelAuthCode() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != State::FETCHING_CODE)
    return;

  DisableArc();
}

void ArcAuthService::EnableArc() {
  DCHECK(thread_checker_.CalledOnValidThread());
  profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
}

void ArcAuthService::DisableArc() {
  DCHECK(thread_checker_.CalledOnValidThread());
  profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
}

void ArcAuthService::OnAuthCodeFetched(const std::string& auth_code) {
  DCHECK_EQ(state_, State::FETCHING_CODE);
  SetAuthCodeAndStartArc(auth_code);
}

void ArcAuthService::ShowUI() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Get auth token to continue.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  CHECK(token_service && signin_manager);
  const std::string& account_id = signin_manager->GetAuthenticatedAccountId();
  ubertoken_fethcher_.reset(
      new UbertokenFetcher(token_service, this, GaiaConstants::kChromeOSSource,
                           storage_partition_->GetURLRequestContext()));
  ubertoken_fethcher_->StartFetchingToken(account_id);
}

void ArcAuthService::OnAuthCodeNeedUI() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (disable_ui_for_testing || IsOptInVerificationDisabled())
    return;

  ShowUI();
}

void ArcAuthService::OnAuthCodeFailed() {
  DCHECK_EQ(state_, State::FETCHING_CODE);

  ShutdownBridgeAndCloseUI();
}

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state) {
  switch (state) {
    case ArcAuthService::State::STOPPED:
      return os << kStateStopped;
    case ArcAuthService::State::FETCHING_CODE:
      return os << kStateFetchingCode;
    case ArcAuthService::State::ACTIVE:
      return os << kStateActive;
    default:
      NOTREACHED();
      return os;
  }
}

}  // namespace arc
