// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
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

Service::Service() : weak_factory_(this) {}

Service::~Service() = default;

void Service::OnStart() {
  RequestAccessToken();
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
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

void Service::GetPrimaryAccountInfoCallback(
    const base::Optional<AccountInfo>& account_info,
    const identity::AccountState& account_state) {
  if (!account_info.has_value() || !account_state.has_refresh_token)
    return;
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(kScopeAuthGcm);
  identity_manager_->GetAccessToken(
      account_info.value().account_id, scopes, "cros_assistant",
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)));
}

// TODO: Handle |expiration_time| and token refreshing.
void Service::GetAccessTokenCallback(const base::Optional<std::string>& token,
                                     base::Time expiration_time,
                                     const GoogleServiceAuthError& error) {
  if (!token.has_value()) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    return;
  }

  if (!assistant_manager_service_) {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    assistant_manager_service_ =
        std::make_unique<AssistantManagerServiceImpl>();
#else
    assistant_manager_service_ =
        std::make_unique<FakeAssistantManagerServiceImpl>();
#endif
    assistant_manager_service_->Start(token.value());
  } else {
    assistant_manager_service_->SetAccessToken(token.value());
  }
}

}  // namespace assistant
}  // namespace chromeos
