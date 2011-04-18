// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/bindings_policy.h"
#include "content/common/property_bag.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

static base::LazyInstance<
    PropertyAccessor<EnterpriseEnrollmentUI::Controller*> >
        g_enrollment_ui_controller_property(base::LINKER_INITIALIZED);

static const char kEnterpriseEnrollmentGaiaLoginPath[] = "gaialogin";

// WebUIMessageHandler implementation which handles events occuring on the page,
// such as the user pressing the signin button.
class EnterpriseEnrollmentMessageHandler : public WebUIMessageHandler {
 public:
  EnterpriseEnrollmentMessageHandler();
  virtual ~EnterpriseEnrollmentMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handlers for WebUI messages.
  void HandleSubmitAuth(const ListValue* args);
  void HandleCancelAuth(const ListValue* args);
  void HandleConfirmationClose(const ListValue* args);

  // Gets the currently installed enrollment controller (if any).
  EnterpriseEnrollmentUI::Controller* GetController();

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentMessageHandler);
};

// A data source that provides the resources for the enterprise enrollment page.
// The enterprise enrollment page requests the HTML and other resources from
// this source.
class EnterpriseEnrollmentDataSource : public ChromeURLDataManager::DataSource {
 public:
  EnterpriseEnrollmentDataSource();

  // DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  virtual ~EnterpriseEnrollmentDataSource();

  // Saves i18n string for |resource_id| to the |key| property of |dictionary|.
  static void AddString(DictionaryValue* dictionary,
                        const std::string& key,
                        int resource_id);
  static void AddString(DictionaryValue* dictionary,
                        const std::string& key,
                        int resource_id,
                        const string16& arg1);

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentDataSource);
};

EnterpriseEnrollmentMessageHandler::EnterpriseEnrollmentMessageHandler() {}

EnterpriseEnrollmentMessageHandler::~EnterpriseEnrollmentMessageHandler() {}

void EnterpriseEnrollmentMessageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "SubmitAuth",
      NewCallback(
          this, &EnterpriseEnrollmentMessageHandler::HandleSubmitAuth));
  web_ui_->RegisterMessageCallback(
      "DialogClose",
      NewCallback(
          this, &EnterpriseEnrollmentMessageHandler::HandleCancelAuth));
  web_ui_->RegisterMessageCallback(
      "confirmationClose",
      NewCallback(
          this, &EnterpriseEnrollmentMessageHandler::HandleConfirmationClose));
}

void EnterpriseEnrollmentMessageHandler::HandleSubmitAuth(
    const ListValue* value) {
  EnterpriseEnrollmentUI::Controller* controller = GetController();
  if (!controller) {
    NOTREACHED();
    return;
  }

  // Value carries single list entry, which is a json-encoded string that
  // contains the auth parameters (see gaia_login.js).
  std::string json_params;
  if (!value->GetString(0, &json_params)) {
    NOTREACHED();
    return;
  }

  // Check the value type.
  scoped_ptr<Value> params(base::JSONReader::Read(json_params, false));
  if (!params.get() || !params->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return;
  }

  // Read the parameters.
  DictionaryValue* params_dict = static_cast<DictionaryValue*>(params.get());
  std::string user;
  std::string pass;
  std::string captcha;
  std::string access_code;
  if (!params_dict->GetString("user", &user) ||
      !params_dict->GetString("pass", &pass) ||
      !params_dict->GetString("captcha", &captcha) ||
      !params_dict->GetString("access_code", &access_code)) {
    NOTREACHED();
    return;
  }

  controller->OnAuthSubmitted(user, pass, captcha, access_code);
}

void EnterpriseEnrollmentMessageHandler::HandleCancelAuth(
    const ListValue* value) {
  EnterpriseEnrollmentUI::Controller* controller = GetController();
  if (!controller) {
    NOTREACHED();
    return;
  }

  controller->OnAuthCancelled();
}

void EnterpriseEnrollmentMessageHandler::HandleConfirmationClose(
    const ListValue* value) {
  EnterpriseEnrollmentUI::Controller* controller = GetController();
  if (!controller) {
    NOTREACHED();
    return;
  }

  controller->OnConfirmationClosed();
}

EnterpriseEnrollmentUI::Controller*
    EnterpriseEnrollmentMessageHandler::GetController() {
  return EnterpriseEnrollmentUI::GetController(web_ui_);
}

EnterpriseEnrollmentDataSource::EnterpriseEnrollmentDataSource()
    : DataSource(chrome::kChromeUIEnterpriseEnrollmentHost,
                 MessageLoop::current()) {}

void EnterpriseEnrollmentDataSource::StartDataRequest(const std::string& path,
                                                      bool is_off_the_record,
                                                      int request_id) {
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  DictionaryValue strings;
  std::string response;
  if (path.empty()) {
    AddString(&strings, "loginHeader",
              IDS_ENTERPRISE_ENROLLMENT_LOGIN_HEADER),
    AddString(&strings, "loginExplain",
              IDS_ENTERPRISE_ENROLLMENT_LOGIN_EXPLAIN,
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    AddString(&strings, "cloudHeader",
              IDS_ENTERPRISE_ENROLLMENT_CLOUD_HEADER),
    AddString(&strings, "cloudExplain",
              IDS_ENTERPRISE_ENROLLMENT_CLOUD_EXPLAIN);
    AddString(&strings, "accesscontrolHeader",
              IDS_ENTERPRISE_ENROLLMENT_ACCESSCONTROL_HEADER),
    AddString(&strings, "accesscontrolExplain",
              IDS_ENTERPRISE_ENROLLMENT_ACCESSCONTROL_EXPLAIN);
    AddString(&strings, "confirmationHeader",
              IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_HEADER);
    AddString(&strings, "confirmationMessage",
              IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_MESSAGE);
    AddString(&strings, "confirmationClose",
              IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_CLOSE);

    static const base::StringPiece html(
        resource_bundle.GetRawDataResource(IDR_ENTERPRISE_ENROLLMENT_HTML));
    SetFontAndTextDirection(&strings);
    response = jstemplate_builder::GetI18nTemplateHtml(html, &strings);
  } else if (path == kEnterpriseEnrollmentGaiaLoginPath) {
    strings.SetString("invalidpasswordhelpurl", "");
    strings.SetString("invalidaccessaccounturl", "");
    strings.SetString("cannotaccessaccount", "");
    strings.SetString("cannotaccessaccounturl", "");
    strings.SetString("createaccount", "");
    strings.SetString("createnewaccounturl", "");
    strings.SetString("getaccesscodehelp", "");
    strings.SetString("getaccesscodeurl", "");

    // None of the strings used here currently have sync-specific wording in
    // them.  We have a unit test to catch if that happens.
    strings.SetString("introduction", "");
    AddString(&strings, "signinprefix", IDS_SYNC_LOGIN_SIGNIN_PREFIX);
    AddString(&strings, "signinsuffix", IDS_SYNC_LOGIN_SIGNIN_SUFFIX);
    AddString(&strings, "cannotbeblank", IDS_SYNC_CANNOT_BE_BLANK);
    AddString(&strings, "emaillabel", IDS_SYNC_LOGIN_EMAIL);
    AddString(&strings, "passwordlabel", IDS_SYNC_LOGIN_PASSWORD);
    AddString(&strings, "invalidcredentials",
              IDS_SYNC_INVALID_USER_CREDENTIALS);
    AddString(&strings, "signin", IDS_SYNC_SIGNIN);
    AddString(&strings, "couldnotconnect", IDS_SYNC_LOGIN_COULD_NOT_CONNECT);
    AddString(&strings, "cancel", IDS_CANCEL);
    AddString(&strings, "settingup", IDS_SYNC_LOGIN_SETTING_UP);
    AddString(&strings, "success", IDS_SYNC_SUCCESS);
    AddString(&strings, "errorsigningin", IDS_SYNC_ERROR_SIGNING_IN);
    AddString(&strings, "captchainstructions",
              IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS);
    AddString(&strings, "invalidaccesscode",
              IDS_SYNC_INVALID_ACCESS_CODE_LABEL);
    AddString(&strings, "enteraccesscode", IDS_SYNC_ENTER_ACCESS_CODE_LABEL);

    static const base::StringPiece html(resource_bundle.GetRawDataResource(
        IDR_GAIA_LOGIN_HTML));
    SetFontAndTextDirection(&strings);
    response = jstemplate_builder::GetI18nTemplateHtml(html, &strings);
  }

  // Send the response.
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes());
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

std::string EnterpriseEnrollmentDataSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}

EnterpriseEnrollmentDataSource::~EnterpriseEnrollmentDataSource() {}

void EnterpriseEnrollmentDataSource::AddString(DictionaryValue* dictionary,
                                               const std::string& key,
                                               int resource_id) {
  dictionary->SetString(key, l10n_util::GetStringUTF16(resource_id));
}

void EnterpriseEnrollmentDataSource::AddString(DictionaryValue* dictionary,
                                               const std::string& key,
                                               int resource_id,
                                               const string16& arg1) {
  dictionary->SetString(key, l10n_util::GetStringFUTF16(resource_id, arg1));
}

EnterpriseEnrollmentUI::EnterpriseEnrollmentUI(TabContents* contents)
    : WebUI(contents) {}

EnterpriseEnrollmentUI::~EnterpriseEnrollmentUI() {}

void EnterpriseEnrollmentUI::RenderViewCreated(
    RenderViewHost* render_view_host) {
  // Bail out early if the controller doesn't exist or web ui is disabled.
  if (!GetController(this) || !(bindings_ & BindingsPolicy::WEB_UI))
    return;

  WebUIMessageHandler* handler = new EnterpriseEnrollmentMessageHandler();
  AddMessageHandler(handler->Attach(this));

  // Set up the data source, so the enrollment page can be loaded.
  tab_contents()->profile()->GetChromeURLDataManager()->AddDataSource(
      new EnterpriseEnrollmentDataSource());

  std::string user;
  bool has_init_user = GetController(this)->GetInitialUser(&user);
  if (!has_init_user)
    user = "";
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("user", user);
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", !has_init_user);
  args.SetString("initialScreen", "login-screen");
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  render_view_host->SetWebUIProperty("dialogArguments", json_args);
}

// static
EnterpriseEnrollmentUI::Controller* EnterpriseEnrollmentUI::GetController(
    WebUI* ui) {
  Controller** controller =
      g_enrollment_ui_controller_property.Get().GetProperty(
          ui->tab_contents()->property_bag());

  return controller ? *controller : NULL;
}

// static
void EnterpriseEnrollmentUI::SetController(
    TabContents* contents,
    EnterpriseEnrollmentUI::Controller* controller) {
  g_enrollment_ui_controller_property.Get().SetProperty(
      contents->property_bag(),
      controller);
}

}  // namespace chromeos
