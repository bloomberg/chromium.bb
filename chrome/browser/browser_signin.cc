// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_signin.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/browser/webui/constrained_html_ui.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/webui/web_ui_util.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

class BrowserSigninResourcesSource : public ChromeURLDataManager::DataSource {
 public:
  BrowserSigninResourcesSource()
      : DataSource(chrome::kChromeUIDialogHost, MessageLoop::current()) {
  }

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string& path) const {
    return "text/html";
  }

 private:
  virtual ~BrowserSigninResourcesSource() {}

  DISALLOW_COPY_AND_ASSIGN(BrowserSigninResourcesSource);
};

void BrowserSigninResourcesSource::StartDataRequest(const std::string& path,
                                                    bool is_off_the_record,
                                                    int request_id) {
  const char kSigninPath[] = "signin";

  std::string response;
  if (path == kSigninPath) {
    const base::StringPiece html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_SIGNIN_HTML));
    DictionaryValue dict;
    SetFontAndTextDirection(&dict);
    response = jstemplate_builder::GetI18nTemplateHtml(html, &dict);
  }

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

class BrowserSigninHtml : public HtmlDialogUIDelegate,
                          public WebUIMessageHandler {
 public:
  BrowserSigninHtml(BrowserSignin* signin,
                    string16 suggested_email,
                    string16 login_message);
  virtual ~BrowserSigninHtml() {}

  // HtmlDialogUIDelegate implementation
  virtual bool IsDialogModal() const {
    return false;
  };
  virtual std::wstring GetDialogTitle() const {
    return L"";
  }
  virtual GURL GetDialogContentURL() const {
    return GURL("chrome://dialog/signin");
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {
    const WebUIMessageHandler* handler = this;
    handlers->push_back(const_cast<WebUIMessageHandler*>(handler));
  }
  virtual void GetDialogSize(gfx::Size* size) const {
    size->set_width(600);
    size->set_height(300);
  }
  virtual std::string GetDialogArgs() const {
    return UTF16ToASCII(login_message_);
  }
  virtual void OnDialogClosed(const std::string& json_retval) {
    closed_ = true;
    signin_->Cancel();
  }
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
  }
  virtual bool ShouldShowDialogTitle() const { return true; }

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Refreshes the UI, such as after an authentication error.
  void ReloadUI();

  // Method which calls into javascript to force the dialog to close.
  void ForceDialogClose();

 private:
  // JS callback handlers.
  void HandleSigninInit(const ListValue* args);
  void HandleSubmitAuth(const ListValue* args);

  // Nonowned pointer; |signin_| owns this object.
  BrowserSignin* signin_;

  string16 suggested_email_;
  string16 login_message_;

  bool closed_;
};

BrowserSigninHtml::BrowserSigninHtml(BrowserSignin* signin,
                                     string16 suggested_email,
                                     string16 login_message)
    : signin_(signin),
      suggested_email_(suggested_email),
      login_message_(login_message),
      closed_(false) {
}

void BrowserSigninHtml::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "SubmitAuth", NewCallback(this, &BrowserSigninHtml::HandleSubmitAuth));
  web_ui_->RegisterMessageCallback(
      "SigninInit", NewCallback(this, &BrowserSigninHtml::HandleSigninInit));
}

void BrowserSigninHtml::ReloadUI() {
  HandleSigninInit(NULL);
}

void BrowserSigninHtml::ForceDialogClose() {
  if (!closed_ && web_ui_) {
    StringValue value("DialogClose");
    ListValue close_args;
    close_args.Append(new StringValue(""));
    web_ui_->CallJavascriptFunction(L"chrome.send", value, close_args);
  }
}

void BrowserSigninHtml::HandleSigninInit(const ListValue* args) {
  if (!web_ui_)
    return;

  RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
  rvh->ExecuteJavascriptInWebFrame(ASCIIToUTF16("//iframe[@id='login']"),
                                   ASCIIToUTF16("hideBlurb();"));

  DictionaryValue json_args;
  std::string json;
  std::wstring javascript(L"");
  SyncSetupFlow::GetArgsForGaiaLogin(signin_->GetProfileSyncService(),
                                     &json_args);

  // Replace the suggested email, unless sync has already required a
  // particular value.
  bool is_editable;
  std::string user;
  json_args.GetBoolean("editable_user", &is_editable);
  json_args.GetString("user", &user);
  if (is_editable && user.empty() && !suggested_email_.empty())
    json_args.SetString("user", suggested_email_);

  base::JSONWriter::Write(&json_args, false, &json);
  javascript += L"showGaiaLogin(" + UTF8ToWide(json) + L");";
  rvh->ExecuteJavascriptInWebFrame(ASCIIToUTF16("//iframe[@id='login']"),
                                   WideToUTF16Hack(javascript));
}

void BrowserSigninHtml::HandleSubmitAuth(const ListValue* args) {
  std::string json(web_ui_util::GetJsonResponseFromFirstArgumentInList(args));
  scoped_ptr<DictionaryValue> result(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));
  std::string username;
  std::string password;
  std::string captcha;
  std::string access_code;
  if (!result.get() ||
      !result->GetString("user", &username) ||
      !result->GetString("pass", &password) ||
      !result->GetString("captcha", &captcha) ||
      !result->GetString("access_code", &access_code)) {
    LOG(ERROR) << "Unintelligble format for authentication data from page.";
    signin_->Cancel();
  }
  signin_->GetProfileSyncService()->OnUserSubmittedAuth(
      username, password, captcha, access_code);
}

BrowserSignin::BrowserSignin(Profile* profile)
    : profile_(profile),
      delegate_(NULL),
      html_dialog_ui_delegate_(NULL) {
  // profile is NULL during testing.
  if (profile) {
    BrowserSigninResourcesSource* source = new BrowserSigninResourcesSource();
    profile->GetChromeURLDataManager()->AddDataSource(source);
  }
}

BrowserSignin::~BrowserSignin() {
  delegate_ = NULL;
}

void BrowserSignin::RequestSignin(TabContents* tab_contents,
                                  const string16& suggested_email,
                                  const string16& login_message,
                                  SigninDelegate* delegate) {
  CHECK(tab_contents);
  CHECK(delegate);
  // Cancel existing request.
  if (delegate_)
    Cancel();
  delegate_ = delegate;
  suggested_email_ = suggested_email;
  login_message_ = login_message;
  RegisterAuthNotifications();
  ShowSigninTabModal(tab_contents);
}

std::string BrowserSignin::GetSignedInUsername() const {
  std::string username =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  VLOG(1) << "GetSignedInUsername: " << username;
  return username;
}

void BrowserSignin::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::GOOGLE_SIGNIN_SUCCESSFUL: {
      VLOG(1) << "GOOGLE_SIGNIN_SUCCESSFUL";
      if (delegate_)
        delegate_->OnLoginSuccess();
      // Close the dialog.
      OnLoginFinished();
      break;
    }
    case NotificationType::GOOGLE_SIGNIN_FAILED: {
      VLOG(1) << "GOOGLE_SIGNIN_FAILED";
      // The signin failed, refresh the UI with error information.
      html_dialog_ui_delegate_->ReloadUI();
      break;
    }
    default:
      NOTREACHED();
  }
}

void BrowserSignin::Cancel() {
  if (delegate_) {
    delegate_->OnLoginFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::REQUEST_CANCELED));
    GetProfileSyncService()->OnUserCancelledDialog();
  }
  OnLoginFinished();
}

void BrowserSignin::OnLoginFinished() {
  if (html_dialog_ui_delegate_)
    html_dialog_ui_delegate_->ForceDialogClose();
  // The dialog will be deleted by WebUI due to the dialog close,
  // don't hold a reference.
  html_dialog_ui_delegate_ = NULL;

  if (delegate_) {
    UnregisterAuthNotifications();
    delegate_ = NULL;
  }
}

ProfileSyncService* BrowserSignin::GetProfileSyncService() const {
  return profile_->GetProfileSyncService();
}

BrowserSigninHtml* BrowserSignin::CreateHtmlDialogUI() {
  return new BrowserSigninHtml(this, suggested_email_, login_message_);
}

void BrowserSignin::RegisterAuthNotifications() {
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
                 Source<Profile>(profile_));
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_FAILED,
                 Source<Profile>(profile_));
}

void BrowserSignin::UnregisterAuthNotifications() {
  registrar_.Remove(this,
                    NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
                    Source<Profile>(profile_));
  registrar_.Remove(this,
                    NotificationType::GOOGLE_SIGNIN_FAILED,
                    Source<Profile>(profile_));
}

void BrowserSignin::ShowSigninTabModal(TabContents* tab_contents) {
//  TODO(johnnyg): Need a linux views implementation for ConstrainedHtmlDialog.
#if defined(OS_WIN) || defined(OS_CHROMEOS) || !defined(TOOLKIT_VIEWS)
  html_dialog_ui_delegate_ = CreateHtmlDialogUI();
  ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile_,
                                                 html_dialog_ui_delegate_,
                                                 tab_contents);
#endif
}
