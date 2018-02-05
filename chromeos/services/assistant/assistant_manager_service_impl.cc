// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace assistant {

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    AuthTokenProvider* auth_token_provider)
    : auth_token_provider_(auth_token_provider),
      platform_api_(kDefaultConfigStr),
      assistant_manager_(
          assistant_client::AssistantManager::Create(&platform_api_,
                                                     kDefaultConfigStr)),
      assistant_manager_internal_(
          UnwrapAssistantManagerInternal(assistant_manager_.get())) {}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {}

void AssistantManagerServiceImpl::Start() {
  auto* internal_options =
      assistant_manager_internal_->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options);
  assistant_manager_internal_->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });

  // By default AssistantManager will not start listening for its hotword until
  // we explicitly set EnableListening() to |true|.
  assistant_manager_->EnableListening(true);

  // Push the |access_token| we got as an argument into AssistantManager before
  // starting to ensure that all server requests will be authenticated once
  // it is started. |user_id| is used to pair a user to their |access_token|,
  // since we do not support multi-user in this example we can set it to a
  // dummy value like "0".
  assistant_manager_->SetAuthTokens({std::pair<std::string, std::string>(
      /* user_id: */ "0", auth_token_provider_->GetAccessToken())});

  assistant_manager_->Start();
}

}  // namespace assistant
}  // namespace chromeos
