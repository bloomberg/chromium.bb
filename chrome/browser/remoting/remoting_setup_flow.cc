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
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_process_type.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

// Use static Run method to get an instance.
RemotingSetupFlow::RemotingSetupFlow(const std::string& args, Profile* profile)
    : message_handler_(NULL),
      dialog_start_args_(args),
      profile_(profile),
      process_control_(NULL) {
  // TODO(hclam): We are currently leaking this objcet. Need to fix this!
  message_handler_ = new RemotingSetupMessageHandler(this);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new RemotingResourcesSource())));
}

RemotingSetupFlow::~RemotingSetupFlow() {
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
  delete this;
}

void RemotingSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked.
  handlers->push_back(message_handler_);
}

void RemotingSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
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

void RemotingSetupFlow::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  message_handler_->ShowGaiaFailed();
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
  message_handler_->ShowGaiaSuccessAndSettingUp();

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
    OnProcessLaunched();
  } else {
    // TODO(hclam): This is really messed up because I totally ignore the
    // lifetime of this object. I need a master cleanup on the lifetime of
    // objects.
    // TODO(hclam): This call only works on Windows. I need to make it work
    // on other platforms.
    process_control_->Launch(
        NewRunnableMethod(this, &RemotingSetupFlow::OnProcessLaunched));
  }
}

void RemotingSetupFlow::OnIssueAuthTokenFailure(const std::string& service,
    const GoogleServiceAuthError& error) {
  // TODO(hclam): Do something to show the error.
  authenticator_.reset();
}

void RemotingSetupFlow::OnProcessLaunched() {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI,
        FROM_HERE,
        NewRunnableMethod(this, &RemotingSetupFlow::OnProcessLaunched));
    return;
  }

  DCHECK(process_control_->is_connected());
  process_control_->EnableRemotingWithTokens(login_, remoting_token_,
                                             sync_token_);
  message_handler_->ShowSetupDone();

  // Save the preference that we have completed the setup of remoting.
  profile_->GetPrefs()->SetBoolean(prefs::kRemotingHasSetupCompleted, true);
}

// static
RemotingSetupFlow* RemotingSetupFlow::Run(Profile* profile) {
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

  RemotingSetupFlow* flow = new RemotingSetupFlow(json_args, profile);
  Browser* b = BrowserList::GetLastActive();
  if (b) {
    b->BrowserShowHtmlDialog(flow, NULL);
  } else {
    delete flow;
    return NULL;
  }
  return flow;
}

// Global function to start the remoting setup dialog.
void OpenRemotingSetupDialog(Profile* profile) {
  RemotingSetupFlow::Run(profile->GetOriginalProfile());
}

// TODO(hclam): Need to refcount RemotingSetupFlow. I need to fix
// lifetime of objects.
DISABLE_RUNNABLE_METHOD_REFCOUNT(RemotingSetupFlow);
DISABLE_RUNNABLE_METHOD_REFCOUNT(RemotingSetupMessageHandler);
