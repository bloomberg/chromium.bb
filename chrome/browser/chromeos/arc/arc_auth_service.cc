// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include <utility>

#include "base/command_line.h"
#include "chrome/browser/chromeos/arc/arc_auth_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcAuthService* arc_auth_service = nullptr;

// Skip creating UI in unit tests
bool disable_ui_for_testing = false;

const char kStateDisable[] = "DISABLE";
const char kStateFetchingCode[] = "FETCHING_CODE";
const char kStateNoCode[] = "NO_CODE";
const char kStateEnable[] = "ENABLE";
}  // namespace

ArcAuthService::ArcAuthService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  DCHECK(!arc_auth_service);
  arc_auth_service = this;

  arc_bridge_service()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  DCHECK(!auth_ui_ && !profile_);
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
  callback.Run(mojo::String(GetAndResetAuthCode()),
               !IsOptInVerificationDisabled());
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

  profile_ = profile;

  // In case UI is disabled we assume that ARC is opted-in.
  if (!IsOptInVerificationDisabled()) {
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kArcEnabled,
        base::Bind(&ArcAuthService::OnOptInPreferenceChanged,
                   base::Unretained(this)));
    OnOptInPreferenceChanged();
  } else {
    SetAuthCodeAndStartArc(std::string());
  }
}

void ArcAuthService::Shutdown() {
  ShutdownBridgeAndCloseUI();
  profile_ = nullptr;
  pref_change_registrar_.RemoveAll();
}

void ArcAuthService::OnOptInPreferenceChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(profile_);

  if (profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    switch (state_) {
      case State::DISABLE:
        FetchAuthCode();
        break;
      case State::NO_CODE:  // Retry
        FetchAuthCode();
        break;
      default:
        break;
    }
  } else {
    ShutdownBridgeAndCloseUI();
  }
}

void ArcAuthService::ShutdownBridgeAndCloseUI() {
  CloseUI();
  auth_fetcher_.reset();
  ArcBridgeService::Get()->Shutdown();
  SetState(State::DISABLE);
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
  if (auth_ui_) {
    auth_ui_->Close();
    DCHECK(!auth_ui_);
  }
}

void ArcAuthService::SetAuthCodeAndStartArc(const std::string& auth_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!auth_code.empty() || IsOptInVerificationDisabled());
  DCHECK_NE(state_, State::ENABLE);

  ShutdownBridgeAndCloseUI();

  auth_code_ = auth_code;
  ArcBridgeService::Get()->HandleStartup();

  SetState(State::ENABLE);
}

void ArcAuthService::FetchAuthCode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == State::DISABLE || state_ == State::NO_CODE);

  CloseUI();
  auth_code_.clear();

  SetState(State::FETCHING_CODE);

  auth_fetcher_.reset(new ArcAuthFetcher(profile_->GetRequestContext(), this));
}

void ArcAuthService::OnAuthCodeFetched(const std::string& auth_code) {
  DCHECK_EQ(state_, State::FETCHING_CODE);
  SetAuthCodeAndStartArc(auth_code);
}

void ArcAuthService::OnAuthCodeNeedUI() {
  CloseUI();
  if (!disable_ui_for_testing && !IsOptInVerificationDisabled())
    auth_ui_ = new ArcAuthUI(profile_, this);
}

void ArcAuthService::OnAuthCodeFailed() {
  DCHECK_EQ(state_, State::FETCHING_CODE);
  CloseUI();

  SetState(State::NO_CODE);
}

void ArcAuthService::OnAuthUIClosed() {
  DCHECK(auth_ui_);
  auth_ui_ = nullptr;
}

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state) {
  switch (state) {
    case ArcAuthService::State::DISABLE:
      return os << kStateDisable;
    case ArcAuthService::State::FETCHING_CODE:
      return os << kStateFetchingCode;
    case ArcAuthService::State::NO_CODE:
      return os << kStateNoCode;
    case ArcAuthService::State::ENABLE:
      return os << kStateEnable;
    default:
      NOTREACHED();
      return os;
  }
}

}  // namespace arc
