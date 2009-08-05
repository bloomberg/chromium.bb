// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/sync/sync_setup_flow.h"
#include "chrome/common/pref_service.h"

static const int kSyncDialogWidth = 267;
static const int kSyncDialogHeight = 369;

// XPath expression for finding specific iframes.
static const wchar_t* kLoginIFrameXPath = L"//iframe[@id='login']";
static const wchar_t* kMergeIFrameXPath = L"//iframe[@id='merge']";

// Helper function to read the JSON string from the Value parameter.
static std::string GetJsonResponse(const Value* content) {
  if (!content || !content->IsType(Value::TYPE_LIST))  {
    NOTREACHED();
    return std::string();
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() != 1) {
    NOTREACHED();
    return std::string();
  }

  std::string result;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&result)) {
    NOTREACHED();
    return std::string();
  }

  return result;
}

void FlowHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &FlowHandler::HandleSubmitAuth));
  dom_ui_->RegisterMessageCallback("SubmitMergeAndSync",
      NewCallback(this, &FlowHandler::HandleSubmitMergeAndSync));
}

static bool GetUsernameAndPassword(const std::string& json,
    std::string* username, std::string* password) {
  scoped_ptr<Value> parsed_value(JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString(L"user", username) ||
      !result->GetString(L"pass", password)) {
      return false;
  }
  return true;
}

void FlowHandler::HandleSubmitAuth(const Value* value) {
  std::string json(GetJsonResponse(value));
  std::string username, password;
  if (json.empty())
    return;

  if (!GetUsernameAndPassword(json, &username, &password)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  if (flow_)
    flow_->OnUserSubmittedAuth(username, password);
}

void FlowHandler::HandleSubmitMergeAndSync(const Value* value) {
  if (flow_)
    flow_->OnUserAcceptedMergeAndSync();
}

// Called by SyncSetupFlow::Advance.
void FlowHandler::ShowGaiaLogin(const DictionaryValue& args) {
  std::string json;
  JSONWriter::Write(&args, false, &json);
  std::wstring javascript = std::wstring(L"showGaiaLogin") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kLoginIFrameXPath, javascript);
}

void FlowHandler::ShowGaiaSuccessAndClose() {
  ExecuteJavascriptInIFrame(kLoginIFrameXPath, L"showGaiaSuccessAndClose();");
}

void FlowHandler::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(kLoginIFrameXPath,
                            L"showGaiaSuccessAndSettingUp();");
}

void FlowHandler::ShowMergeAndSync() {
  if (dom_ui_)  // NULL during testing.
    dom_ui_->CallJavascriptFunction(L"showMergeAndSync");
}

void FlowHandler::ShowMergeAndSyncDone() {
  ExecuteJavascriptInIFrame(kMergeIFrameXPath, L"showMergeAndSyncDone();");
}

void FlowHandler::ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                            const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}

SyncSetupFlow::~SyncSetupFlow() {
  flow_handler_->set_flow(NULL);
}

void SyncSetupFlow::GetDialogSize(gfx::Size* size) const {
  size->set_width(kSyncDialogWidth);
  size->set_height(kSyncDialogHeight);
}

// A callback to notify the delegate that the dialog closed.
void SyncSetupFlow::OnDialogClosed(const std::string& json_retval) {
  DCHECK(json_retval.empty());
  container_->set_flow(NULL);  // Sever ties from the wizard.
  if (current_state_ == SyncSetupWizard::DONE) {
    PrefService* prefs = service_->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kSyncHasSetupCompleted, true);
    prefs->ScheduleSavePersistentPrefs();
  }
  service_->OnUserCancelledDialog();
  delete this;
}

// static
void SyncSetupFlow::GetArgsForGaiaLogin(const ProfileSyncService* service,
                                        DictionaryValue* args) {
  AuthErrorState error(service->GetAuthErrorState());
  if (!service->last_attempted_user_email().empty()) {
    args->SetString(L"user", service->last_attempted_user_email());
    args->SetInteger(L"error", error);
  } else {
    std::wstring user(UTF16ToWide(service->GetAuthenticatedUsername()));
    args->SetString(L"user", user);
    args->SetInteger(L"error", user.empty() ? 0 : error);
  }
}

void SyncSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  handlers->push_back(flow_handler_);
}

bool SyncSetupFlow::ShouldAdvance(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::GAIA_LOGIN:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::GAIA_SUCCESS:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::MERGE_AND_SYNC:
      return current_state_ == SyncSetupWizard::GAIA_SUCCESS;
    case SyncSetupWizard::DONE:
      return current_state_ == SyncSetupWizard::MERGE_AND_SYNC ||
             current_state_ == SyncSetupWizard::GAIA_SUCCESS;
    default:
      NOTREACHED() << "Unhandled State: " << state;
      return false;
  }
}

void SyncSetupFlow::Advance(SyncSetupWizard::State advance_state) {
  if (!ShouldAdvance(advance_state))
    return;
  switch (advance_state) {
    case SyncSetupWizard::GAIA_LOGIN: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
    case SyncSetupWizard::GAIA_SUCCESS:
      if (end_state_ == SyncSetupWizard::GAIA_SUCCESS)
        flow_handler_->ShowGaiaSuccessAndClose();
      else
        flow_handler_->ShowGaiaSuccessAndSettingUp();
      break;
    case SyncSetupWizard::MERGE_AND_SYNC:
      flow_handler_->ShowMergeAndSync();
      break;
    case SyncSetupWizard::DONE:
      if (current_state_ == SyncSetupWizard::MERGE_AND_SYNC)
        flow_handler_->ShowMergeAndSyncDone();
      else if (current_state_ == SyncSetupWizard::GAIA_SUCCESS)
        flow_handler_->ShowGaiaSuccessAndClose();
      break;
    default:
      NOTREACHED() << "Invalid advance state: " << advance_state;
  }
  current_state_ = advance_state;
}


// static
SyncSetupFlow* SyncSetupFlow::Run(ProfileSyncService* service,
                                  SyncSetupFlowContainer* container,
                                  SyncSetupWizard::State start,
                                  SyncSetupWizard::State end) {
  DictionaryValue args;
  if (start == SyncSetupWizard::GAIA_LOGIN)
    SyncSetupFlow::GetArgsForGaiaLogin(service, &args);
  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  Browser* b = BrowserList::GetLastActive();
  if (!b)
    return NULL;

  FlowHandler* handler = new FlowHandler();
  SyncSetupFlow* flow = new SyncSetupFlow(start, end, json_args,
      container, handler, service);
  handler->set_flow(flow);
  b->BrowserShowHtmlDialog(flow, NULL);
  return flow;
}

#endif  // CHROME_PERSONALIZATION
