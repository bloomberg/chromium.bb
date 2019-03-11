// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"

namespace autofill_assistant {

// Implementation of ScriptExecutorDelegate that's convenient to use in
// unittests.
class FakeScriptExecutorDelegate : public ScriptExecutorDelegate {
 public:
  FakeScriptExecutorDelegate();
  ~FakeScriptExecutorDelegate() override;

  const GURL& GetCurrentURL() override;
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const std::map<std::string, std::string>& GetParameters() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void EnterState(AutofillAssistantState state) override;
  void SetTouchableElementArea(const ElementAreaProto& element) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetDetails(const Details& details) override;
  void ClearDetails() override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  void SetProgress(int progress) override;
  void SetProgressVisible(bool visible) override;
  void SetChips(std::unique_ptr<std::vector<Chip>> chips) override;
  void SetPaymentRequestOptions(
      std::unique_ptr<PaymentRequestOptions> options) override;

  void SetCurrentURL(const GURL& url) { current_url_ = url; }

  void SetService(Service* service) { service_ = service; }

  void SetUiController(UiController* ui_controller) {
    ui_controller_ = ui_controller;
  }

  void SetWebController(WebController* web_controller) {
    web_controller_ = web_controller;
  }

  std::map<std::string, std::string>* GetMutableParameters() {
    return &parameters_;
  }

  AutofillAssistantState GetState() { return state_; }

  Details* GetDetails() { return details_.get(); }

  InfoBox* GetInfoBox() { return info_box_.get(); }

  std::vector<Chip>* GetChips() { return chips_.get(); }

  PaymentRequestOptions* GetOptions() { return payment_request_options_.get(); }

 private:
  GURL current_url_;
  Service* service_ = nullptr;
  UiController* ui_controller_ = nullptr;
  WebController* web_controller_ = nullptr;
  ClientMemory memory_;
  std::map<std::string, std::string> parameters_;
  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;
  std::string status_message_;
  std::unique_ptr<Details> details_;
  std::unique_ptr<InfoBox> info_box_;
  std::unique_ptr<std::vector<Chip>> chips_;
  std::unique_ptr<PaymentRequestOptions> payment_request_options_;

  DISALLOW_COPY_AND_ASSIGN(FakeScriptExecutorDelegate);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
