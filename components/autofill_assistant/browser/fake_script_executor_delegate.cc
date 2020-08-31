// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"

#include <utility>

namespace autofill_assistant {

FakeScriptExecutorDelegate::FakeScriptExecutorDelegate()
    : trigger_context_(TriggerContext::CreateEmpty()) {}

FakeScriptExecutorDelegate::~FakeScriptExecutorDelegate() = default;

const ClientSettings& FakeScriptExecutorDelegate::GetSettings() {
  return client_settings_;
}

const GURL& FakeScriptExecutorDelegate::GetCurrentURL() {
  return current_url_;
}

const GURL& FakeScriptExecutorDelegate::GetDeeplinkURL() {
  return current_url_;
}

const GURL& FakeScriptExecutorDelegate::GetScriptURL() {
  return current_url_;
}

Service* FakeScriptExecutorDelegate::GetService() {
  return service_;
}

WebController* FakeScriptExecutorDelegate::GetWebController() {
  return web_controller_;
}

TriggerContext* FakeScriptExecutorDelegate::GetTriggerContext() {
  return trigger_context_.get();
}

autofill::PersonalDataManager*
FakeScriptExecutorDelegate::GetPersonalDataManager() {
  return nullptr;
}

WebsiteLoginManager* FakeScriptExecutorDelegate::GetWebsiteLoginManager() {
  return nullptr;
}

content::WebContents* FakeScriptExecutorDelegate::GetWebContents() {
  return nullptr;
}

std::string FakeScriptExecutorDelegate::GetEmailAddressForAccessTokenAccount() {
  return std::string();
}

std::string FakeScriptExecutorDelegate::GetLocale() {
  return "en-US";
}

bool FakeScriptExecutorDelegate::EnterState(AutofillAssistantState state) {
  if (GetState() == state)
    return false;

  state_history_.emplace_back(state);
  return true;
}

void FakeScriptExecutorDelegate::SetTouchableElementArea(
    const ElementAreaProto& element) {}

void FakeScriptExecutorDelegate::SetStatusMessage(const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorDelegate::GetStatusMessage() const {
  return status_message_;
}

void FakeScriptExecutorDelegate::SetBubbleMessage(const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorDelegate::GetBubbleMessage() const {
  return status_message_;
}

void FakeScriptExecutorDelegate::SetDetails(std::unique_ptr<Details> details) {
  details_ = std::move(details);
}

void FakeScriptExecutorDelegate::SetInfoBox(const InfoBox& info_box) {
  info_box_ = std::make_unique<InfoBox>(info_box);
}

void FakeScriptExecutorDelegate::ClearInfoBox() {
  info_box_ = nullptr;
}

void FakeScriptExecutorDelegate::SetProgress(int progress) {}

void FakeScriptExecutorDelegate::SetProgressVisible(bool visible) {}

void FakeScriptExecutorDelegate::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  user_actions_ = std::move(user_actions);
}

void FakeScriptExecutorDelegate::SetCollectUserDataOptions(
    CollectUserDataOptions* options) {
  payment_request_options_ = options;
}

void FakeScriptExecutorDelegate::WriteUserData(
    base::OnceCallback<void(UserData*, UserData::FieldChange*)>
        write_callback) {
  if (payment_request_options_ == nullptr || payment_request_info_ == nullptr) {
    return;
  }

  UserData::FieldChange field_change = UserData::FieldChange::NONE;
  std::move(write_callback).Run(payment_request_info_.get(), &field_change);
}

void FakeScriptExecutorDelegate::WriteUserModel(
    base::OnceCallback<void(UserModel*)> write_callback) {
  std::move(write_callback).Run(user_model_);
}

void FakeScriptExecutorDelegate::SetViewportMode(ViewportMode mode) {
  viewport_mode_ = mode;
}

ViewportMode FakeScriptExecutorDelegate::GetViewportMode() {
  return viewport_mode_;
}

void FakeScriptExecutorDelegate::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  peek_mode_ = peek_mode;
}

ConfigureBottomSheetProto::PeekMode FakeScriptExecutorDelegate::GetPeekMode() {
  return peek_mode_;
}

void FakeScriptExecutorDelegate::ExpandBottomSheet() {
  expand_or_collapse_updated_ = true;
  expand_or_collapse_value_ = true;
}

void FakeScriptExecutorDelegate::CollapseBottomSheet() {
  expand_or_collapse_updated_ = true;
  expand_or_collapse_value_ = false;
}

bool FakeScriptExecutorDelegate::HasNavigationError() {
  return navigation_error_;
}

bool FakeScriptExecutorDelegate::IsNavigatingToNewDocument() {
  return navigating_to_new_document_;
}

void FakeScriptExecutorDelegate::RequireUI() {
  require_ui_ = true;
}

void FakeScriptExecutorDelegate::AddListener(NavigationListener* listener) {
  listeners_.insert(listener);
}

void FakeScriptExecutorDelegate::RemoveListener(NavigationListener* listener) {
  listeners_.erase(listener);
}

void FakeScriptExecutorDelegate::SetExpandSheetForPromptAction(bool expand) {
  expand_sheet_for_prompt_ = expand;
}

void FakeScriptExecutorDelegate::SetBrowseDomainsWhitelist(
    std::vector<std::string> domains) {
  browse_domains_ = std::move(domains);
}

bool FakeScriptExecutorDelegate::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  return true;
}

UserModel* FakeScriptExecutorDelegate::GetUserModel() {
  return user_model_;
}

EventHandler* FakeScriptExecutorDelegate::GetEventHandler() {
  return nullptr;
}

void FakeScriptExecutorDelegate::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(bool, ProcessedActionStatusProto, const UserModel*)>
        end_action_callback) {}

void FakeScriptExecutorDelegate::ClearGenericUi() {}

}  // namespace autofill_assistant
