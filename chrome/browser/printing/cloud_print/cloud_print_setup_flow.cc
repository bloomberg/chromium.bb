// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"

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
#include "chrome/browser/printing/cloud_print/cloud_print_setup_message_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/remoting/remoting_resources_source.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_type.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

static const wchar_t kLoginIFrameXPath[] = L"//iframe[@id='login']";
static const wchar_t kDoneIframeXPath[] = L"//iframe[@id='done']";

////////////////////////////////////////////////////////////////////////////////
// CloudPrintServiceProcessHelper
//
// This is a helper class to perform actions when the service process
// is connected or launched. The events are sent back to CloudPrintSetupFlow
// when the dialog is still active. CloudPrintSetupFlow can detach from this
// helper class when the dialog is closed.

class CloudPrintServiceProcessHelper
    : public base::RefCountedThreadSafe<CloudPrintServiceProcessHelper> {
 public:
  explicit CloudPrintServiceProcessHelper(CloudPrintSetupFlow* flow)
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
  CloudPrintSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintServiceProcessHelper);
};

////////////////////////////////////////////////////////////////////////////////
// CloudPrintServiceDisableTask
//
// This is a helper class to get the proxy service launched if it
// isn't, in order to properly inform it that it should be disabled.

class CloudPrintServiceDisableTask
    : public base::RefCountedThreadSafe<CloudPrintServiceDisableTask> {
 public:
  explicit CloudPrintServiceDisableTask(Profile* profile)
      : profile_(profile),
        process_control_(NULL) {
  }

  void StartDisable() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    process_control_ =
        ServiceProcessControlManager::instance()->GetProcessControl(
            profile_,
            kServiceProcessCloudPrint);

    if (process_control_) {
      // If the process isn't connected, launch it now.  This will run
      // the task whether the process is already launched or not, as
      // long as it's able to connect back up.
      process_control_->Launch(
          NewRunnableMethod(
              this, &CloudPrintServiceDisableTask::OnProcessLaunched));
    }
  }

  void OnProcessLaunched() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    DCHECK(process_control_);
    if (process_control_->is_connected())
      process_control_->Send(new ServiceMsg_DisableCloudPrintProxy());
    profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
  }

 private:
  Profile* profile_;
  ServiceProcessControl* process_control_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintServiceDisableTask);
};

////////////////////////////////////////////////////////////////////////////////
// CloudPrintServiceRefreshTask
//
// This is a helper class to perform a preferences refresh of the
// enablement state and registered e-mail from the cloud print proxy
// service.

class CloudPrintServiceRefreshTask
    : public base::RefCountedThreadSafe<CloudPrintServiceRefreshTask> {
 public:
  explicit CloudPrintServiceRefreshTask(
      Profile* profile,
      Callback2<bool, std::string>::Type* callback)
      : profile_(profile),
        process_control_(NULL),
        callback_(callback) {
    DCHECK(callback);
  }

  void StartRefresh() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    process_control_ =
        ServiceProcessControlManager::instance()->GetProcessControl(
            profile_,
            kServiceProcessCloudPrint);

    if (process_control_) {
      // If the process isn't connected, launch it now.  This will run
      // the task whether the process is already launched or not, as
      // long as it's able to connect back up.
      process_control_->Launch(
          NewRunnableMethod(
              this, &CloudPrintServiceRefreshTask::OnProcessLaunched));
    }
  }

  void OnProcessLaunched() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    DCHECK(process_control_);

    if (callback_ != NULL)
      process_control_->GetCloudPrintProxyStatus(callback_.release());
  }

 private:
  Profile* profile_;
  ServiceProcessControl* process_control_;

  // Callback that gets invoked when a status message is received from
  // the cloud print proxy.
  scoped_ptr<Callback2<bool, std::string>::Type> callback_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintServiceRefreshTask);
};

////////////////////////////////////////////////////////////////////////////////
// CloudPrintSetupFlow implementation.

// static
CloudPrintSetupFlow* CloudPrintSetupFlow::OpenDialog(Profile* profile) {
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("iframeToShow", "login");
  args.SetString("user", "");
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", true);

  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      !profile->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty()) {
    args.SetString("iframeToShow", "done");
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  // Create a browser to run the dialog.  The new CloudPrintSetupFlow
  // class takes ownership.
  Browser* browser = Browser::CreateForPopup(profile);
  DCHECK(browser);

  CloudPrintSetupFlow* flow = new CloudPrintSetupFlow(json_args, browser);
  browser->BrowserShowHtmlDialog(flow, NULL);
  return flow;
}

// static
void CloudPrintSetupFlow::DisableCloudPrintProxy(Profile* profile) {
  scoped_refptr<CloudPrintServiceDisableTask> refresh_task =
      new CloudPrintServiceDisableTask(profile);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(refresh_task.get(),
                        &CloudPrintServiceDisableTask::StartDisable));
}

// static
void CloudPrintSetupFlow::RefreshPreferencesFromService(
    Profile* profile,
    Callback2<bool, std::string>::Type* callback) {
  scoped_refptr<CloudPrintServiceRefreshTask> refresh_task =
      new CloudPrintServiceRefreshTask(profile, callback);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(refresh_task.get(),
                        &CloudPrintServiceRefreshTask::StartRefresh));
}

CloudPrintSetupFlow::CloudPrintSetupFlow(const std::string& args,
                                         Browser* browser)
    : dom_ui_(NULL),
      dialog_start_args_(args),
      process_control_(NULL) {
  // TODO(hclam): The data source should be added once.
  browser_.reset(browser);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new RemotingResourcesSource())));
}

CloudPrintSetupFlow::~CloudPrintSetupFlow() {
}

void CloudPrintSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation.
GURL CloudPrintSetupFlow::GetDialogContentURL() const {
  return GURL("chrome://remotingresources/setup");
}

void CloudPrintSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked, the caller is
  // responsible for deleting the objects.
  handlers->push_back(
      new CloudPrintSetupMessageHandler(
          const_cast<CloudPrintSetupFlow*>(this)));
}

void CloudPrintSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = browser_->profile()->GetPrefs();
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
void CloudPrintSetupFlow::OnDialogClosed(const std::string& json_retval) {
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

std::string CloudPrintSetupFlow::GetDialogArgs() const {
    return dialog_start_args_;
}

void  CloudPrintSetupFlow::OnCloseContents(TabContents* source,
                                         bool* out_close_dialog) {
}

std::wstring  CloudPrintSetupFlow::GetDialogTitle() const {
  return l10n_util::GetString(IDS_CLOUD_PRINT_SETUP_DIALOG_TITLE);
}

bool  CloudPrintSetupFlow::IsDialogModal() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// GaiaAuthConsumer implementation.
void CloudPrintSetupFlow::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

void CloudPrintSetupFlow::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Save the token for the cloud print proxy.
  lsid_ = credentials.lsid;

  // Show that Gaia login has succeeded.
  ShowGaiaSuccessAndSettingUp();
  authenticator_.reset();

  // And then launch the service process if it has not started yet.
  // If we have already connected to the service process then submit the tokens
  // to it to register the host.
  process_control_ =
      ServiceProcessControlManager::instance()->GetProcessControl(
          browser_->profile(),
          kServiceProcessCloudPrint);

#if defined(OS_WIN)
  // TODO(hclam): This call only works on Windows. I need to make it
  // work on other platforms.
  service_process_helper_ = new CloudPrintServiceProcessHelper(this);

  // If the process isn't connected, launch it now.  This will run the
  // task whether the process is already launched or not, as long as
  // it's able to connect back up.
  process_control_->Launch(
      NewRunnableMethod(service_process_helper_.get(),
                        &CloudPrintServiceProcessHelper::OnProcessLaunched));
#else
  ShowSetupDone();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Methods called by CloudPrintSetupMessageHandler
void CloudPrintSetupFlow::Attach(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;
}

void CloudPrintSetupFlow::OnUserSubmittedAuth(const std::string& user,
                                              const std::string& password,
                                              const std::string& captcha) {
  // Save the login name only.
  login_ = user;

  // Start the authenticator.
  authenticator_.reset(
      new GaiaAuthenticator2(this, GaiaConstants::kChromeSource,
                             browser_->profile()->GetRequestContext()));
  authenticator_->StartClientLogin(user, password,
                                   GaiaConstants::kCloudPrintService,
                                   "", captcha);
}

///////////////////////////////////////////////////////////////////////////////
// Method called by CloudPrintServiceProcessHelper
void CloudPrintSetupFlow::OnProcessLaunched() {
  DCHECK(process_control_->is_connected());
  // TODO(scottbyer): Need to wait for an ACK to be sure that it is
  // actually active.
  if (!lsid_.empty())
    process_control_->Send(new ServiceMsg_EnableCloudPrintProxy(lsid_));

  // Save the preference that we have completed the setup of cloud
  // print.
  browser_->profile()->GetPrefs()->SetString(prefs::kCloudPrintEmail, login_);
  ShowSetupDone();
}

///////////////////////////////////////////////////////////////////////////////
// Helper methods for showing contents of the DOM UI
void CloudPrintSetupFlow::ShowGaiaLogin(const DictionaryValue& args) {
  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showGaiaLoginIframe");

  std::string json;
  base::JSONWriter::Write(&args, false, &json);
  std::wstring javascript = std::wstring(L"showGaiaLogin") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kLoginIFrameXPath, javascript);
}

void CloudPrintSetupFlow::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(kLoginIFrameXPath,
                            L"showGaiaSuccessAndSettingUp();");
}

void CloudPrintSetupFlow::ShowGaiaFailed(const GoogleServiceAuthError& error) {
  DictionaryValue args;
  args.SetString("iframeToShow", "login");
  args.SetString("user", "");
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", true);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  ShowGaiaLogin(args);
}

void CloudPrintSetupFlow::ShowSetupDone() {
  std::wstring javascript = L"setMessage('You are all set!');";
  ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showSetupDone");

  ExecuteJavascriptInIFrame(kDoneIframeXPath, L"onPageShown();");
}

void CloudPrintSetupFlow::ExecuteJavascriptInIFrame(
    const std::wstring& iframe_xpath,
    const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}
