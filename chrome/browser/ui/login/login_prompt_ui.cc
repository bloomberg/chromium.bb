// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
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
                                int request_id) {
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

    // Send the response.
    scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
    html_bytes->data.resize(response.size());
    std::copy(response.begin(), response.end(), html_bytes->data.begin());
    SendResponse(request_id, html_bytes);
  }

  virtual std::string GetMimeType(const std::string& path) const {
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
                           TabContents *tab_contents,
                           const string16 explanation)
      : login_handler_(login_handler),
        tab_contents_(tab_contents),
        explanation_(UTF16ToUTF8(explanation)),
        closed_(false),
        has_autofill_(false),
        ready_for_autofill_(false) {
  }

  // HtmlDialogUIDelegate methods:
  virtual bool IsDialogModal() const {
    return true;
  }

  virtual std::wstring GetDialogTitle() const {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE));
  }

  virtual GURL GetDialogContentURL() const {
    return GURL(chrome::kChromeUIHttpAuthURL);
  }

  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {
    const WebUIMessageHandler* handler = this;
    handlers->push_back(const_cast<WebUIMessageHandler*>(handler));
  }

  virtual void GetDialogSize(gfx::Size* size) const {
    size->set_width(kDialogWidth);
    size->set_height(kDialogHeight);
  }

  virtual std::string GetDialogArgs() const {
    return explanation_;
  }

  virtual void OnDialogClosed(const std::string& json_retval);

  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {}

  virtual bool ShouldShowDialogTitle() const {
    return true;
  }

  // WebUIMessageHandler method:
  virtual void RegisterMessages() {
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
  TabContents *tab_contents_;
  std::string explanation_;
  bool closed_;

  bool has_autofill_;
  bool ready_for_autofill_;
  std::string autofill_username_;
  std::string autofill_password_;

  static const int kDialogWidth = 400;
  static const int kDialogHeight = 130;

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
                                       const std::wstring& password) {
    if (delegate_)
      delegate_->ShowAutofillData(username, password);
  }

  // LoginHandler method:
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           const string16& explanation);

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
  if (closed_)
    return;
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

  LOG(INFO) << "BuildViewForPasswordManager";

  TabContents* tab_contents = GetTabContentsForLogin();
  LoginHandlerSource::RegisterDataSource(tab_contents->profile());
  delegate_ = new LoginHandlerHtmlDelegate(this, tab_contents, explanation);
  ConstrainedWindow* dialog = ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
      tab_contents->profile(), delegate_, tab_contents);

  SetModel(manager);
  SetDialog(dialog);

  NotifyAuthNeeded();
}

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerHtml(auth_info, request);
}
