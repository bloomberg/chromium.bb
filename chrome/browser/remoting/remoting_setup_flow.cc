// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_setup_flow.h"

#include "app/gfx/font_util.h"
#include "base/json/json_writer.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/remoting/remoting_resources_source.h"
#include "chrome/browser/remoting/remoting_setup_message_handler.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_process_type.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

static const wchar_t kLoginIFrameXPath[] = L"//iframe[@id='login']";
static const wchar_t kDoneIframeXPath[] = L"//iframe[@id='done']";

////////////////////////////////////////////////////////////////////////////////
// RemotingServiceProcessHelper
//
// This is a helper class to perform actions when the service process
// is connected or launched. The events are sent back to RemotingSetupFlow
// when the dialog is still active. RemotingSetupFlow can detach from this
// helper class when the dialog is closed.
class RemotingServiceProcessHelper :
    public base::RefCountedThreadSafe<RemotingServiceProcessHelper> {
 public:
  RemotingServiceProcessHelper(RemotingSetupFlow* flow)
      : flow_(flow) {
  }

  void Detach() {
    flow_ = NULL;
  }

  void OnProcessLaunched() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    // If the flow is detached then show the done page.
    if (!flow_)
      return;

    flow_->OnProcessLaunched();
  }

 private:
  RemotingSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(RemotingServiceProcessHelper);
};

////////////////////////////////////////////////////////////////////////////////
// RemotingSetupFlow implementation.
// static
RemotingSetupFlow* RemotingSetupFlow::OpenDialog(Profile* profile) {
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("iframeToShow", "login");
  args.SetString("user", "");
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", true);

  if (profile->GetPrefs()->GetBoolean(prefs::kRemotingHasSetupCompleted)) {
    args.SetString("iframeToShow", "done");
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  Browser* b = BrowserList::GetLastActive();
  if (!b)
    return NULL;

  RemotingSetupFlow* flow = new RemotingSetupFlow(json_args, profile);
  b->BrowserShowHtmlDialog(flow, NULL);
  return flow;
}

RemotingSetupFlow::RemotingSetupFlow(const std::string& args, Profile* profile)
    : dom_ui_(NULL),
      dialog_start_args_(args),
      profile_(profile),
      process_control_(NULL) {
  // TODO(hclam): The data source should be added once.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new RemotingResourcesSource())));
}

RemotingSetupFlow::~RemotingSetupFlow() {
}

void RemotingSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation.
GURL RemotingSetupFlow::GetDialogContentURL() const {
  return GURL("chrome://remotingresources/setup");
}

void RemotingSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked, the caller is
  // responsible for deleting the objects.
  handlers->push_back(
      new RemotingSetupMessageHandler(const_cast<RemotingSetupFlow*>(this)));
}

void RemotingSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font(
      UTF8ToWide(prefs->GetString(prefs::kWebKitSansSerifFontFamily)),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  // TODO(pranavk) Replace the following SYNC resources with REMOTING Resources.
  *size = gfx::GetLocalizedContentsSizeForFont(
      IDS_SYNC_SETUP_WIZARD_WIDTH_CHARS,
      IDS_SYNC_SETUP_WIZARD_HEIGHT_LINES,
      approximate_web_font);
}

// A callback to notify the delegate that the dialog closed.
void RemotingSetupFlow::OnDialogClosed(const std::string& json_retval) {
  // If we are fetching the token then cancel the request.
  if (authenticator_.get())
    authenticator_->CancelRequest();

  // If the service process helper is still active then detach outself from it.
  // This is because the dialog is closing and this object is going to be
  // deleted but the service process launch is still in progress so we don't
  // the service process helper to call us when the process is launched.
  if (service_process_helper_.get())
    service_process_helper_->Detach();
  delete this;
}

std::string RemotingSetupFlow::GetDialogArgs() const {
    return dialog_start_args_;
}

void  RemotingSetupFlow::OnCloseContents(TabContents* source,
                                         bool* out_close_dialog) {
}

std::wstring  RemotingSetupFlow::GetDialogTitle() const {
  return l10n_util::GetString(IDS_REMOTING_SETUP_DIALOG_TITLE);
}

bool  RemotingSetupFlow::IsDialogModal() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// GaiaAuthConsumer implementation.
void RemotingSetupFlow::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

void RemotingSetupFlow::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Save the token for remoting.
  remoting_token_ = credentials.token;

  // After login has succeeded try to fetch the token for sync.
  // We need the token for sync to connect to the talk network.
  authenticator_->StartIssueAuthToken(credentials.sid, credentials.lsid,
                                      GaiaConstants::kSyncService);
}

void RemotingSetupFlow::OnIssueAuthTokenSuccess(const std::string& service,
                                                const std::string& auth_token) {
  // Show that Gaia login has succeeded.
  ShowGaiaSuccessAndSettingUp();

  // Save the sync token.
  sync_token_ = auth_token;
  authenticator_.reset();

  // And then launch the service process if it has not started yet.
  // If we have already connected to the service process then submit the tokens
  // to it to register the host.
  process_control_ =
      ServiceProcessControlManager::instance()->GetProcessControl(
          profile_,
          kServiceProcessRemoting);

  if (process_control_->is_connected()) {
    // TODO(hclam): Need to figure out what to do when the service process is
    // already connected.
  } else {
#if defined(OS_WIN)
    // TODO(hclam): This call only works on Windows. I need to make it work
    // on other platforms.
    service_process_helper_ = new RemotingServiceProcessHelper(this);
    process_control_->Launch(
        NewRunnableMethod(service_process_helper_.get(),
                          &RemotingServiceProcessHelper::OnProcessLaunched));
#else
    ShowSetupDone();
#endif
  }
}

void RemotingSetupFlow::OnIssueAuthTokenFailure(const std::string& service,
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// Methods called by RemotingSetupMessageHandler
void RemotingSetupFlow::Attach(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;
}

void RemotingSetupFlow::OnUserSubmittedAuth(const std::string& user,
                                            const std::string& password,
                                            const std::string& captcha) {
  // Save the login name only.
  login_ = user;

  // Start the authenticator.
  authenticator_.reset(
      new GaiaAuthenticator2(this, GaiaConstants::kChromeSource,
                             profile_->GetRequestContext()));
  authenticator_->StartClientLogin(user, password,
                                   GaiaConstants::kRemotingService,
                                   "", captcha);
}

///////////////////////////////////////////////////////////////////////////////
// Method called by RemotingServicePRocessHelper
void RemotingSetupFlow::OnProcessLaunched() {
  DCHECK(process_control_->is_connected());
  // TODO(hclam): Need to wait for an ACK to be sure that it is actually active.
  process_control_->EnableRemotingWithTokens(login_, remoting_token_,
                                             sync_token_);

  // Save the preference that we have completed the setup of remoting.
  profile_->GetPrefs()->SetBoolean(prefs::kRemotingHasSetupCompleted, true);
  ShowSetupDone();
}

///////////////////////////////////////////////////////////////////////////////
// Helper methods for showing contents of the DOM UI
void RemotingSetupFlow::ShowGaiaLogin(const DictionaryValue& args) {
  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showGaiaLoginIframe");

  std::string json;
  base::JSONWriter::Write(&args, false, &json);
  std::wstring javascript = std::wstring(L"showGaiaLogin") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kLoginIFrameXPath, javascript);
}

void RemotingSetupFlow::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(kLoginIFrameXPath,
                            L"showGaiaSuccessAndSettingUp();");
}

void RemotingSetupFlow::ShowGaiaFailed(const GoogleServiceAuthError& error) {
  DictionaryValue args;
  args.SetString("iframeToShow", "login");
  args.SetString("user", "");
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", true);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  ShowGaiaLogin(args);
}

void RemotingSetupFlow::ShowSetupDone() {
   std::wstring javascript = L"setMessage('You are all set!');";
   ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showSetupDone");

   ExecuteJavascriptInIFrame(kDoneIframeXPath, L"onPageShown();");
}

void RemotingSetupFlow::ExecuteJavascriptInIFrame(
    const std::wstring& iframe_xpath,
    const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}
