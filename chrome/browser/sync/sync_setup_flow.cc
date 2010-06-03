// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_flow.h"

#include "app/gfx/font_util.h"
#include "base/callback.h"
#include "base/histogram.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#if defined(OS_MACOSX)
#include "chrome/browser/cocoa/html_dialog_window_controller_cppsafe.h"
#endif
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

// XPath expression for finding specific iframes.
static const wchar_t* kLoginIFrameXPath = L"//iframe[@id='login']";
static const wchar_t* kDoneIframeXPath = L"//iframe[@id='done']";

void FlowHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("ShowCustomize",
      NewCallback(this, &FlowHandler::HandleUserClickedCustomize));
  // On OS X, the customize dialog is modal to the HTML window so we
  // don't need to hook up the two functions below.
#if defined(OS_WIN) || defined(OS_LINUX)
  dom_ui_->RegisterMessageCallback("ClickCustomizeOk",
      NewCallback(this, &FlowHandler::ClickCustomizeOk));
  dom_ui_->RegisterMessageCallback("ClickCustomizeCancel",
      NewCallback(this, &FlowHandler::ClickCustomizeCancel));
#endif
  dom_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &FlowHandler::HandleSubmitAuth));
}

static bool GetAuthData(const std::string& json,
    std::string* username, std::string* password, std::string* captcha) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString(L"user", username) ||
      !result->GetString(L"pass", password) ||
      !result->GetString(L"captcha", captcha)) {
      return false;
  }
  return true;
}

void FlowHandler::HandleUserClickedCustomize(const Value* value) {
  if (flow_)
    flow_->OnUserClickedCustomize();
}

// To simulate the user clicking "OK" or "Cancel" on the Customize Sync dialog
void FlowHandler::ClickCustomizeOk(const Value* value) {
  if (flow_)
    flow_->ClickCustomizeOk();
}

void FlowHandler::ClickCustomizeCancel(const Value* value) {
  if (flow_)
    flow_->ClickCustomizeCancel();
}


void FlowHandler::HandleSubmitAuth(const Value* value) {
  std::string json(dom_ui_util::GetJsonResponseFromFirstArgumentInList(value));
  std::string username, password, captcha;
  if (json.empty())
    return;

  // If ClickOk() returns false (indicating that there's a problem in the
  // CustomizeSyncWindowView), don't do anything; the CSWV will focus itself,
  // indicating that there's something to do there.
  // ClickOk() has no side effects if the singleton dialog is not present.
  if (!flow_->ClickCustomizeOk()) {
    // TODO(dantasse): this results in a kinda ugly experience for this edge
    // case; come back here and add a nice message explaining that you can't
    // sync zero datatypes.  (OR just make the CSWV modal to the Gaia Login
    // box, like we want to do anyway.
    flow_->Advance(SyncSetupWizard::GAIA_LOGIN);
    return;
  }

  if (!GetAuthData(json, &username, &password, &captcha)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  if (flow_)
    flow_->OnUserSubmittedAuth(username, password, captcha);
}

// Called by SyncSetupFlow::Advance.
void FlowHandler::ShowGaiaLogin(const DictionaryValue& args) {
  std::string json;
  base::JSONWriter::Write(&args, false, &json);
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

void FlowHandler::ShowSetupDone(const std::wstring& user) {
  StringValue synced_to_string(WideToUTF8(l10n_util::GetStringF(
      IDS_SYNC_NTP_SYNCED_TO, user)));
  std::string json;
  base::JSONWriter::Write(&synced_to_string, false, &json);
  std::wstring javascript = std::wstring(L"setSyncedToUser") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showSetupDone", synced_to_string);

  ExecuteJavascriptInIFrame(kDoneIframeXPath,
                            L"onPageShown();");
}

void FlowHandler::ShowFirstTimeDone(const std::wstring& user) {
  ExecuteJavascriptInIFrame(kDoneIframeXPath,
                            L"setShowFirstTimeSetupSummary();");
  ShowSetupDone(user);
}

void FlowHandler::ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                            const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}

// Use static Run method to get an instance.
SyncSetupFlow::SyncSetupFlow(SyncSetupWizard::State start_state,
                             SyncSetupWizard::State end_state,
                             const std::string& args,
                             SyncSetupFlowContainer* container,
                             ProfileSyncService* service)
    : container_(container),
      dialog_start_args_(args),
      current_state_(start_state),
      end_state_(end_state),
      login_start_time_(base::TimeTicks::Now()),
      flow_handler_(new FlowHandler()),
      owns_flow_handler_(true),
      service_(service),
      html_dialog_window_(NULL) {
  flow_handler_->set_flow(this);
}

SyncSetupFlow::~SyncSetupFlow() {
  flow_handler_->set_flow(NULL);
  if (owns_flow_handler_) {
    delete flow_handler_;
  }
}

void SyncSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = service_->profile()->GetPrefs();
  gfx::Font approximate_web_font = gfx::Font::CreateFont(
      prefs->GetString(prefs::kWebKitSansSerifFontFamily),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  *size = gfx::GetLocalizedContentsSizeForFont(
      IDS_SYNC_SETUP_WIZARD_WIDTH_CHARS,
      IDS_SYNC_SETUP_WIZARD_HEIGHT_LINES,
      approximate_web_font);

#if defined(OS_MACOSX)
  // NOTE(akalin): This is a hack to work around a problem with font height on
  // windows.  Basically font metrics are incorrectly returned in logical units
  // instead of pixels on Windows.  Logical units are very commonly 96 DPI
  // so our localized char/line counts are too small by a factor of 96/72.
  // So we compensate for this on non-windows platform.
  //
  // TODO(akalin): Remove this hack once we fix the windows font problem (or at
  // least work around it in some other place).
  float scale_hack = 96.f/72.f;
  size->set_width(size->width() * scale_hack);
  size->set_height(size->height() * scale_hack);
#endif
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
  const GoogleServiceAuthError& error = service->GetAuthError();
  if (!service->last_attempted_user_email().empty()) {
    args->SetString(L"user", service->last_attempted_user_email());
    args->SetInteger(L"error", error.state());
  } else {
    std::wstring user(UTF16ToWide(service->GetAuthenticatedUsername()));
    args->SetString(L"user", user);
    args->SetInteger(L"error", user.empty() ? 0 : error.state());
  }

  args->SetString(L"captchaUrl", error.captcha().image_url.spec());

  args->SetBoolean(L"showCustomize", true);
}

void SyncSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  handlers->push_back(flow_handler_);
  // We don't own flow_handler_ anymore, but it sticks around until at least
  // right after OnDialogClosed() is called (and this object is destroyed).
  owns_flow_handler_ = false;
}

bool SyncSetupFlow::ShouldAdvance(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::GAIA_LOGIN:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::GAIA_SUCCESS:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::FATAL_ERROR:
      return true;  // You can always hit the panic button.
    case SyncSetupWizard::DONE_FIRST_TIME:
    case SyncSetupWizard::DONE:
      return current_state_ == SyncSetupWizard::GAIA_SUCCESS;
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
    case SyncSetupWizard::FATAL_ERROR: {
      // This shows the user the "Could not connect to server" error.
      // TODO(sync): Update this error messaging.
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      args.SetInteger(L"error", GoogleServiceAuthError::CONNECTION_FAILED);
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
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

void SyncSetupFlow::Focus() {
#if defined(OS_MACOSX)
  if (html_dialog_window_) {
    platform_util::ActivateWindow(html_dialog_window_);
  }
#else
  // TODO(csilv): We don't currently have a way to get the reference to the
  // dialog on windows/linux.  This can be resolved by a cross platform
  // implementation of HTML dialogs as described by akalin below.
  NOTIMPLEMENTED();
#endif  // defined(OS_MACOSX)
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
  base::JSONWriter::Write(&args, false, &json_args);

  SyncSetupFlow* flow = new SyncSetupFlow(start, end, json_args,
      container, service);
#if defined(OS_MACOSX)
  // TODO(akalin): Figure out a cleaner way to do this than to have this
  // gross per-OS behavior, i.e. have a cross-platform ShowHtmlDialog()
  // function that is not tied to a browser instance.  Note that if we do
  // that, we'll have to fix sync_setup_wizard_unittest.cc as it relies on
  // being able to intercept ShowHtmlDialog() calls.
  flow->html_dialog_window_ =
      html_dialog_window_controller::ShowHtmlDialog(
          flow, service->profile());
#else
  Browser* b = BrowserList::GetLastActive();
  if (b) {
    b->BrowserShowHtmlDialog(flow, NULL);
  } else {
    delete flow;
    return NULL;
  }
#endif  // defined(OS_MACOSX)
  return flow;
}
