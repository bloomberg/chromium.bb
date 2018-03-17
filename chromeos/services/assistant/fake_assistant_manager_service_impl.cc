// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceImpl::FakeAssistantManagerServiceImpl() = default;

FakeAssistantManagerServiceImpl::~FakeAssistantManagerServiceImpl() = default;

void FakeAssistantManagerServiceImpl::Start(const std::string& access_token) {
  running_ = true;
}

void FakeAssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {}

void FakeAssistantManagerServiceImpl::EnableListening(bool enable) {}

bool FakeAssistantManagerServiceImpl::IsRunning() const {
  return running_;
}

void FakeAssistantManagerServiceImpl::SendTextQuery(const std::string& query) {}

void FakeAssistantManagerServiceImpl::AddAssistantEventSubscriber(
    mojom::AssistantEventSubscriberPtr subscriber) {}

}  // namespace assistant
}  // namespace chromeos
