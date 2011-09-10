// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"

class LoginHandlerSource : public ChromeURLDataManager::DataSource {
 public:
  LoginHandlerSource()
      : DataSource(chrome::kChromeUIHttpAuthHost, MessageLoop::current()) {}

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id) OVERRIDE {
    DictionaryValue dict;
    dict.SetString("username",
                   l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD));
    dict.SetString("password",
                   l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD));
    dict.SetString("signin",
                   l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL));
    dict.SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));

    SetFontAndTextDirection(&dict);

    const base::StringPiece html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_HTTP_AUTH_HTML));
    std::string response = jstemplate_builder::GetI18nTemplateHtml(html, &dict);

    SendResponse(request_id, base::RefCountedString::TakeString(&response));
  }

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE {
    return "text/html";
  }

  static void RegisterDataSource(Profile *profile) {
    ChromeURLDataManager* url_manager = profile->GetChromeURLDataManager();
    LoginHandlerSource *source = new LoginHandlerSource();
    url_manager->AddDataSource(source);
  }

 private:
  virtual ~LoginHandlerSource() {}

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerSource);
};

class LoginHandlerHtml;

class LoginHandlerHtmlDelegate : public HtmlDialogUIDelegate,
                                 public WebUIMessageHandler {
 public:
  LoginHandlerHtmlDelegate(LoginHandlerHtml *login_handler,
                           const string16 explanation)
      : login_handler_(login_handler),
        explanation_(UTF16ToUTF8(explanation)),
        closed_(false),
        has_autofill_(false),
        ready_for_autofill_(false) {
  }

  // HtmlDialogUIDelegate methods:
  virtual bool IsDialogModal() const OVERRIDE {
    return true;
  }

  virtual string16 GetDialogTitle() const OVERRIDE {
    return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
  }

  virtual GURL GetDialogContentURL() const OVERRIDE {
    return GURL(chrome::kChromeUIHttpAuthURL);
  }

  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE {
    const WebUIMessageHandler* handler = this;
    handlers->push_back(const_cast<WebUIMessageHandler*>(handler));
  }

  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE {
    size->set_width(kDialogWidth);
    size->set_height(kDialogHeight);
  }

  virtual std::string GetDialogArgs() const OVERRIDE {
    return explanation_;
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;

  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE {}

  virtual bool ShouldShowDialogTitle() const OVERRIDE {
    return true;
  }

  // WebUIMessageHandler method:
  virtual void RegisterMessages() OVERRIDE {
    web_ui_->RegisterMessageCallback(
        "GetAutofill",
        NewCallback(this, &LoginHandlerHtmlDelegate::GetAutofill));
  }

  void ShowAutofillData(const std::wstring& username,
                        const std::wstring& password);

 private:
  // Send autofill data to HTML once the dialog is ready and the data is
  // available.
  void SendAutofillData();

  // Handle the request for autofill data from HTML.
  void GetAutofill(const ListValue* args) {
    ready_for_autofill_ = true;
    SendAutofillData();
  }

  LoginHandlerHtml *login_handler_;
  std::string explanation_;
  bool closed_;

  bool has_autofill_;
  bool ready_for_autofill_;
  std::string autofill_username_;
  std::string autofill_password_;

  static const int kDialogWidth = 400;
  static const int kDialogHeight = 160;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerHtmlDelegate);
};

class LoginHandlerHtml : public LoginHandler {
 public:
  LoginHandlerHtml(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request),
        delegate_(NULL) {
  }

  // LoginModelObserver method:
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) OVERRIDE {
    if (delegate_)
      delegate_->ShowAutofillData(username, password);
  }

  // LoginHandler method:
  virtual void BuildViewForPasswordManager(
      PasswordManager* manager, const string16& explanation) OVERRIDE;

  friend class LoginHandlerHtmlDelegate;

 protected:
  virtual ~LoginHandlerHtml() {}

 private:
  LoginHandlerHtmlDelegate *delegate_;

  void FreeAndRelease() {
    SetDialog(NULL);
    SetModel(NULL);
    ReleaseSoon();
  }

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerHtml);
};

void LoginHandlerHtmlDelegate::OnDialogClosed(const std::string& json_retval) {
  DCHECK(!closed_);
  closed_ = true;

  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json_retval, false));

  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    login_handler_->CancelAuth();
  } else {
    DictionaryValue* res = static_cast<DictionaryValue*>(parsed_value.get());
    string16 username;
    string16 password;

    if (!res->GetString("username", &username) ||
        !res->GetString("password", &password)) {
      login_handler_->CancelAuth();
    } else {
      login_handler_->SetAuth(username, password);
    }
  }

  login_handler_->FreeAndRelease();

  // We don't need to delete |this| here: the WebUI object will delete us since
  // we've registered ourselves as a WebUIMessageHandler.
}

void LoginHandlerHtmlDelegate::ShowAutofillData(const std::wstring& username,
                                                const std::wstring& password) {
  autofill_username_ = WideToUTF8(username);
  autofill_password_ = WideToUTF8(password);
  has_autofill_ = true;
  SendAutofillData();
}

void LoginHandlerHtmlDelegate::SendAutofillData() {
  if (!closed_ && web_ui_ && has_autofill_ && ready_for_autofill_) {
    StringValue username_v(autofill_username_);
    StringValue password_v(autofill_password_);
    web_ui_->CallJavascriptFunction("setAutofillCredentials",
                                    username_v, password_v);
  }
}

void LoginHandlerHtml::BuildViewForPasswordManager(
    PasswordManager* manager, const string16& explanation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents = GetTabContentsForLogin();
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  Profile* profile = wrapper->profile();
  LoginHandlerSource::RegisterDataSource(profile);
  delegate_ = new LoginHandlerHtmlDelegate(this, explanation);
  ConstrainedWindow* dialog = ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
      profile, delegate_, wrapper);

  SetModel(manager);
  SetDialog(dialog);

  NotifyAuthNeeded();
}

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerHtml(auth_info, request);
}
