// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"

#include <utility>

namespace autofill_assistant {

FakeScriptExecutorDelegate::FakeScriptExecutorDelegate() = default;
FakeScriptExecutorDelegate::~FakeScriptExecutorDelegate() = default;

const GURL& FakeScriptExecutorDelegate::GetCurrentURL() {
  return current_url_;
}

Service* FakeScriptExecutorDelegate::GetService() {
  return service_;
}

UiController* FakeScriptExecutorDelegate::GetUiController() {
  return ui_controller_;
}

WebController* FakeScriptExecutorDelegate::GetWebController() {
  return web_controller_;
}

ClientMemory* FakeScriptExecutorDelegate::GetClientMemory() {
  return &memory_;
}

const std::map<std::string, std::string>&
FakeScriptExecutorDelegate::GetParameters() {
  return parameters_;
}

autofill::PersonalDataManager*
FakeScriptExecutorDelegate::GetPersonalDataManager() {
  return nullptr;
}

content::WebContents* FakeScriptExecutorDelegate::GetWebContents() {
  return nullptr;
}

void FakeScriptExecutorDelegate::EnterState(AutofillAssistantState state) {
  state_ = state;
}

void FakeScriptExecutorDelegate::SetTouchableElementArea(
    const ElementAreaProto& element) {}

void FakeScriptExecutorDelegate::SetStatusMessage(const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorDelegate::GetStatusMessage() const {
  return status_message_;
}

void FakeScriptExecutorDelegate::SetDetails(const Details& details) {
  details_ = std::make_unique<Details>(details);
}

void FakeScriptExecutorDelegate::ClearDetails() {
  details_ = nullptr;
}

void FakeScriptExecutorDelegate::SetProgress(int progress) {}

void FakeScriptExecutorDelegate::SetProgressVisible(bool visible) {}

void FakeScriptExecutorDelegate::SetChips(
    std::unique_ptr<std::vector<Chip>> chips) {
  chips_ = std::move(chips);
}

void FakeScriptExecutorDelegate::SetPaymentRequestOptions(
    std::unique_ptr<PaymentRequestOptions> options) {
  payment_request_options_ = std::move(options);
}
}  // namespace autofill_assistant
