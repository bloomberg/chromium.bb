// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace assistant {

AssistantManagerServiceImpl::AssistantManagerServiceImpl()
    : platform_api_(kDefaultConfigStr),
      action_module_(std::make_unique<action::CrosActionModule>()),
      assistant_manager_(
          assistant_client::AssistantManager::Create(&platform_api_,
                                                     kDefaultConfigStr)),
      assistant_manager_internal_(
          UnwrapAssistantManagerInternal(assistant_manager_.get())) {
  display_connection_.SetAssistantEventObserver(this);
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {}

void AssistantManagerServiceImpl::Start(const std::string& access_token) {
  auto* internal_options =
      assistant_manager_internal_->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options);
  assistant_manager_internal_->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
  assistant_manager_internal_->SetDisplayConnection(&display_connection_);

  assistant_manager_internal_->RegisterActionModule(action_module_.get());

  SetAccessToken(access_token);
  assistant_manager_->Start();
}

void AssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {
  // Push the |access_token| we got as an argument into AssistantManager before
  // starting to ensure that all server requests will be authenticated once
  // it is started. |user_id| is used to pair a user to their |access_token|,
  // since we do not support multi-user in this example we can set it to a
  // dummy value like "0".
  assistant_manager_->SetAuthTokens({std::pair<std::string, std::string>(
      /* user_id: */ "0", access_token)});
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  assistant_manager_->EnableListening(enable);
}

void AssistantManagerServiceImpl::SendTextQuery(const std::string& query) {
  assistant_manager_internal_->SendTextQuery(query);
}

void AssistantManagerServiceImpl::AddAssistantEventSubscriber(
    mojom::AssistantEventSubscriberPtr subscriber) {
  subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html) {
  subscribers_.ForAllPtrs([&html](auto* ptr) { ptr->OnHtmlResponse(html); });
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  subscribers_.ForAllPtrs([&text](auto* ptr) { ptr->OnTextResponse(text); });
}

}  // namespace assistant
}  // namespace chromeos
