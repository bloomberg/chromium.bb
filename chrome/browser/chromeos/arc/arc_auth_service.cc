// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include <utility>

#include "chrome/browser/chromeos/arc/arc_auth_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_bridge_service.h"

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
  arc_bridge_service()->RemoveObserver(this);
  CloseUI();

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
void ArcAuthService::DisableUIForTesting() {
  disable_ui_for_testing = true;
}

void ArcAuthService::OnAuthInstanceReady() {
  arc_bridge_service()->auth_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

std::string ArcAuthService::GetAndResetAutoCode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string auth_code;
  auth_code_.swap(auth_code);
  return auth_code;
}

void ArcAuthService::GetAuthCode(const GetAuthCodeCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(mojo::String(GetAndResetAutoCode()));
}

void ArcAuthService::SetState(State state) {
  DCHECK_NE(state_, state);
  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInChanged(state_));
}

void ArcAuthService::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK(profile && profile != profile_);
  DCHECK(thread_checker_.CalledOnValidThread());

  Shutdown();

  profile_ = profile;

  // TODO(khmel). At this moment UI to handle ARC OptIn is not ready yet. Assume
  // we opted in by default. When UI is ready, this should be synced with
  // user's prefs.
  FetchAuthCode();
}

void ArcAuthService::Shutdown() {
  profile_ = nullptr;
  ArcBridgeService::Get()->Shutdown();
  if (state_ != State::DISABLE) {
    auth_fetcher_.reset();
    SetState(State::DISABLE);
  }
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
  DCHECK(!auth_code.empty());
  DCHECK_NE(state_, State::ENABLE);

  CloseUI();
  auth_fetcher_.reset();
  auth_code_ = auth_code;
  ArcBridgeService::Get()->HandleStartup();

  SetState(State::ENABLE);
}

void ArcAuthService::FetchAuthCode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::DISABLE);

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
  if (!disable_ui_for_testing)
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
