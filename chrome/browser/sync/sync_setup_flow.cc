// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(BROWSER_SYNC)

#include "chrome/browser/sync/sync_setup_flow.h"

#include "app/gfx/font.h"
#include "app/gfx/font_util.h"
#include "base/histogram.h"
#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "grit/locale_settings.h"

// XPath expression for finding specific iframes.
static const wchar_t* kLoginIFrameXPath = L"//iframe[@id='login']";
static const wchar_t* kMergeIFrameXPath = L"//iframe[@id='merge']";
static const wchar_t* kDoneIframeXPath = L"//iframe[@id='done']";

// Helper function to read the JSON string from the Value parameter.
static std::string GetJsonResponse(const Value* content) {
  if (!content || !content->IsType(Value::TYPE_LIST)) {
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

void FlowHandler::ShowSetupDone(const std::wstring& user) {
  StringValue synced_to_string(WideToUTF8(l10n_util::GetStringF(
      IDS_SYNC_NTP_SYNCED_TO, user)));
  std::string json;
  JSONWriter::Write(&synced_to_string, false, &json);
  std::wstring javascript = std::wstring(L"setSyncedToUser") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showSetupDone", synced_to_string);
}

void FlowHandler::ShowFirstTimeDone(const std::wstring& user) {
  ExecuteJavascriptInIFrame(kDoneIframeXPath,
                            L"setShowFirstTimeSetupSummary();");
  ShowSetupDone(user);
}

void FlowHandler::ShowMergeAndSyncError() {
  ExecuteJavascriptInIFrame(kMergeIFrameXPath, L"showMergeAndSyncError();");
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
  PrefService* prefs = service_->profile()->GetPrefs();
  gfx::Font approximate_web_font = gfx::Font::CreateFont(
      prefs->GetString(prefs::kWebKitSansSerifFontFamily),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  gfx::Size s = gfx::GetLocalizedContentsSizeForFont(
      IDS_SYNC_SETUP_WIZARD_WIDTH_CHARS,
      IDS_SYNC_SETUP_WIZARD_HEIGHT_LINES,
      approximate_web_font);

  size->set_width(s.width());
  size->set_height(s.height());
}

// A callback to notify the delegate that the dialog closed.
void SyncSetupFlow::OnDialogClosed(const std::string& json_retval) {
  DCHECK(json_retval.empty());
  container_->set_flow(NULL);  // Sever ties from the wizard.
  if (current_state_ == SyncSetupWizard::DONE ||
      current_state_ == SyncSetupWizard::DONE_FIRST_TIME) {
    service_->SetSyncSetupCompleted();
  }

  // Record the state at which the user cancelled the signon dialog.
  switch (current_state_) {
    case SyncSetupWizard::GAIA_LOGIN:
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_FROM_SIGNON_WITHOUT_AUTH);
      break;
    case SyncSetupWizard::GAIA_SUCCESS:
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_DURING_SIGNON);
      break;
    case SyncSetupWizard::MERGE_AND_SYNC:
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_DURING_SIGNON_AFTER_MERGE);
      break;
    case SyncSetupWizard::DONE_FIRST_TIME:
    case SyncSetupWizard::DONE:
      UMA_HISTOGRAM_MEDIUM_TIMES("Sync.UserPerceivedAuthorizationTime",
                                 base::TimeTicks::Now() - login_start_time_);
      break;
    default:
      break;
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
    case SyncSetupWizard::FATAL_ERROR:
      return true;  // You can always hit the panic button.
    case SyncSetupWizard::DONE_FIRST_TIME:
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
    case SyncSetupWizard::FATAL_ERROR:
      if (current_state_ == SyncSetupWizard::MERGE_AND_SYNC)
        flow_handler_->ShowMergeAndSyncError();
      break;
    case SyncSetupWizard::DONE_FIRST_TIME:
      flow_handler_->ShowFirstTimeDone(
          UTF16ToWide(service_->GetAuthenticatedUsername()));
      break;
    case SyncSetupWizard::DONE:
      flow_handler_->ShowSetupDone(
          UTF16ToWide(service_->GetAuthenticatedUsername()));
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

#endif  // defined(BROWSER_SYNC)
