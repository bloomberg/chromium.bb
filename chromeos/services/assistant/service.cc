// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <utility>

#include "ash/public/interfaces/ash_assistant_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#else
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#endif

namespace chromeos {
namespace assistant {

namespace {

constexpr char kScopeAuthGcm[] = "https://www.googleapis.com/auth/gcm";
constexpr char kScopeAssistant[] =
    "https://www.googleapis.com/auth/assistant-sdk-prototype";

}  // namespace

Service::Service()
    : platform_binding_(this),
      session_observer_binding_(this),
      token_refresh_timer_(std::make_unique<base::OneShotTimer>()) {
  registry_.AddInterface<mojom::AssistantPlatform>(base::BindRepeating(
      &Service::BindAssistantPlatformConnection, base::Unretained(this)));
}

Service::~Service() = default;

void Service::SetIdentityManagerForTesting(
    identity::mojom::IdentityManagerPtr identity_manager) {
  identity_manager_ = std::move(identity_manager);
}

void Service::SetAssistantManagerForTesting(
    std::unique_ptr<AssistantManagerService> assistant_manager_service) {
  assistant_manager_service_ = std::move(assistant_manager_service);
}

void Service::SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) {
  token_refresh_timer_ = std::move(timer);
}

void Service::OnStart() {}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void Service::BindAssistantConnection(mojom::AssistantRequest request) {
  // Assistant interface is supposed to be used when UI is actually in
  // use, which should be way later than assistant is created.
  DCHECK(assistant_manager_service_);
  bindings_.AddBinding(assistant_manager_service_.get(), std::move(request));
}

void Service::BindAssistantPlatformConnection(
    mojom::AssistantPlatformRequest request) {
  platform_binding_.Bind(std::move(request));
}

void Service::OnSessionActivated(bool activated) {
  DCHECK(client_);
  client_->OnAssistantStatusChanged(activated);
  assistant_manager_service_->EnableListening(activated);
}

void Service::RequestAccessToken() {
  GetIdentityManager()->GetPrimaryAccountInfo(base::BindOnce(
      &Service::GetPrimaryAccountInfoCallback, base::Unretained(this)));
}

identity::mojom::IdentityManager* Service::GetIdentityManager() {
  if (!identity_manager_) {
    context()->connector()->BindInterface(
        identity::mojom::kServiceName, mojo::MakeRequest(&identity_manager_));
  }
  return identity_manager_.get();
}

void Service::Init(mojom::ClientPtr client, mojom::AudioInputPtr audio_input) {
  client_ = std::move(client);
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  assistant_manager_service_ =
      std::make_unique<AssistantManagerServiceImpl>(std::move(audio_input));

  // Bind to Assistant controller in ash.
  ash::mojom::AshAssistantControllerPtr assistant_controller;
  context()->connector()->BindInterface(ash::mojom::kServiceName,
                                        &assistant_controller);
  mojom::AssistantPtr ptr;
  BindAssistantConnection(mojo::MakeRequest(&ptr));
  assistant_controller->SetAssistant(std::move(ptr));
#else
  assistant_manager_service_ =
      std::make_unique<FakeAssistantManagerServiceImpl>();
#endif
  RequestAccessToken();
}

void Service::GetPrimaryAccountInfoCallback(
    const base::Optional<AccountInfo>& account_info,
    const identity::AccountState& account_state) {
  if (!account_info.has_value() || !account_state.has_refresh_token ||
      account_info.value().gaia.empty()) {
    return;
  }
  account_id_ = AccountId::FromUserEmailGaiaId(account_info.value().email,
                                               account_info.value().gaia);
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(kScopeAuthGcm);
  identity_manager_->GetAccessToken(
      account_info.value().account_id, scopes, "cros_assistant",
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)));
}

void Service::GetAccessTokenCallback(const base::Optional<std::string>& token,
                                     base::Time expiration_time,
                                     const GoogleServiceAuthError& error) {
  if (!token.has_value()) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    return;
  }

  DCHECK(assistant_manager_service_);
  if (!assistant_manager_service_->IsRunning()) {
    assistant_manager_service_->Start(token.value());
    AddAshSessionObserver();
    registry_.AddInterface<mojom::Assistant>(base::BindRepeating(
        &Service::BindAssistantConnection, base::Unretained(this)));
    client_->OnAssistantStatusChanged(true);
    DVLOG(1) << "Assistant started";
  } else {
    assistant_manager_service_->SetAccessToken(token.value());
  }

  token_refresh_timer_->Start(FROM_HERE, expiration_time - base::Time::Now(),
                              this, &Service::RequestAccessToken);
}

void Service::AddAshSessionObserver() {
  ash::mojom::SessionControllerPtr session_controller;
  context()->connector()->BindInterface(ash::mojom::kServiceName,
                                        &session_controller);
  ash::mojom::SessionActivationObserverPtr observer;
  session_observer_binding_.Bind(mojo::MakeRequest(&observer));
  session_controller->AddSessionActivationObserverForAccountId(
      account_id_, std::move(observer));
}

}  // namespace assistant
}  // namespace chromeos
