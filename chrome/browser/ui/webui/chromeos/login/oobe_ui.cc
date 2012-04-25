// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enterprise_oauth_enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/user_image_source2.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;

namespace {

// Path for a stripped down login page that does not have OOBE elements.
const char kLoginPath[] = "login";

// Path for the enterprise enrollment gaia page hosting.
const char kEnterpriseEnrollmentGaiaLoginPath[] = "gaialogin";

}  // namespace

namespace chromeos {

class OobeUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit OobeUIHTMLSource(DictionaryValue* localized_strings);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~OobeUIHTMLSource() {}

  std::string GetDataResource(int resource_id) const;

  scoped_ptr<DictionaryValue> localized_strings_;
  DISALLOW_COPY_AND_ASSIGN(OobeUIHTMLSource);
};

// OobeUIHTMLSource -------------------------------------------------------

OobeUIHTMLSource::OobeUIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUIOobeHost, MessageLoop::current()),
      localized_strings_(localized_strings) {
}

void OobeUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_incognito,
                                        int request_id) {
  if (UserManager::Get()->IsUserLoggedIn() &&
      !UserManager::Get()->IsLoggedInAsStub() &&
      !ScreenLocker::default_screen_locker()) {
    scoped_refptr<base::RefCountedBytes> empty_bytes =
        new base::RefCountedBytes();
    SendResponse(request_id, empty_bytes);
    return;
  }

  std::string response;
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    response = GetDataResource(IDR_DEMO_USER_LOGIN_HTML);
  else if (path.empty())
    response = GetDataResource(IDR_OOBE_HTML);
  else if (path == kLoginPath)
    response = GetDataResource(IDR_LOGIN_HTML);
  else if (path == kEnterpriseEnrollmentGaiaLoginPath)
    response = GetDataResource(IDR_GAIA_LOGIN_HTML);

  SendResponse(request_id, base::RefCountedString::TakeString(&response));
}

std::string OobeUIHTMLSource::GetDataResource(int resource_id) const {
  const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  return jstemplate_builder::GetI18nTemplateHtml(html,
                                                 localized_strings_.get());
}

// OobeUI ----------------------------------------------------------------------

OobeUI::OobeUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      update_screen_actor_(NULL),
      network_screen_actor_(NULL),
      eula_screen_actor_(NULL),
      signin_screen_handler_(NULL),
      user_image_screen_actor_(NULL) {
  core_handler_ = new CoreOobeHandler(this);
  AddScreenHandler(core_handler_);

  AddScreenHandler(new NetworkDropdownHandler);

  NetworkScreenHandler* network_screen_handler = new NetworkScreenHandler();
  network_screen_actor_ = network_screen_handler;
  AddScreenHandler(network_screen_handler);

  EulaScreenHandler* eula_screen_handler = new EulaScreenHandler();
  eula_screen_actor_ = eula_screen_handler;
  AddScreenHandler(eula_screen_handler);

  UpdateScreenHandler* update_screen_handler = new UpdateScreenHandler();
  update_screen_actor_ = update_screen_handler;
  AddScreenHandler(update_screen_handler);

  EnterpriseOAuthEnrollmentScreenHandler*
      enterprise_oauth_enrollment_screen_handler =
          new EnterpriseOAuthEnrollmentScreenHandler;
  enterprise_enrollment_screen_actor_ =
      enterprise_oauth_enrollment_screen_handler;
  AddScreenHandler(enterprise_oauth_enrollment_screen_handler);

  UserImageScreenHandler* user_image_screen_handler =
      new UserImageScreenHandler();
  user_image_screen_actor_ = user_image_screen_handler;
  AddScreenHandler(user_image_screen_handler);

  signin_screen_handler_ = new SigninScreenHandler;
  AddScreenHandler(signin_screen_handler_);

  DictionaryValue* localized_strings = new DictionaryValue();
  GetLocalizedStrings(localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://theme/ source, for Chrome logo.
  ThemeSource* theme = new ThemeSource(profile);
  ChromeURLDataManager::AddDataSource(profile, theme);

  // Set up the chrome://terms/ data source, for EULA content.
  AboutUIHTMLSource* about_source =
      new AboutUIHTMLSource(chrome::kChromeUITermsHost, profile);
  ChromeURLDataManager::AddDataSource(profile, about_source);

  // Set up the chrome://oobe/ source.
  OobeUIHTMLSource* html_source = new OobeUIHTMLSource(localized_strings);
  ChromeURLDataManager::AddDataSource(profile, html_source);

  // Set up the chrome://userimage/ source.
  options2::UserImageSource* user_image_source =
      new options2::UserImageSource();
  ChromeURLDataManager::AddDataSource(profile, user_image_source);
}

OobeUI::~OobeUI() {
}

void OobeUI::ShowScreen(WizardScreen* screen) {
  screen->Show();
}

void OobeUI::HideScreen(WizardScreen* screen) {
  screen->Hide();
}

UpdateScreenActor* OobeUI::GetUpdateScreenActor() {
  return update_screen_actor_;
}

NetworkScreenActor* OobeUI::GetNetworkScreenActor() {
  return network_screen_actor_;
}

EulaScreenActor* OobeUI::GetEulaScreenActor() {
  return eula_screen_actor_;
}

EnterpriseEnrollmentScreenActor* OobeUI::
    GetEnterpriseEnrollmentScreenActor() {
  return enterprise_enrollment_screen_actor_;
}

UserImageScreenActor* OobeUI::GetUserImageScreenActor() {
  return user_image_screen_actor_;
}

ViewScreenDelegate* OobeUI::GetRegistrationScreenActor() {
  NOTIMPLEMENTED();
  return NULL;
}

ViewScreenDelegate* OobeUI::GetHTMLPageScreenActor() {
  // WebUI implementation of the LoginDisplayHost opens HTML page directly,
  // without opening OOBE page.
  NOTREACHED();
  return NULL;
}

void OobeUI::GetLocalizedStrings(base::DictionaryValue* localized_strings) {
  // Note, handlers_[0] is a GenericHandler used by the WebUI.
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->GetLocalizedStrings(localized_strings);
  ChromeURLDataManager::DataSource::SetFontAndTextDirection(localized_strings);
}

void OobeUI::AddScreenHandler(BaseScreenHandler* handler) {
  web_ui()->AddMessageHandler(handler);
  handlers_.push_back(handler);
}

void OobeUI::InitializeHandlers() {
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->InitializeBase();
}

void OobeUI::ShowOobeUI(bool show) {
  core_handler_->ShowOobeUI(show);
}

void OobeUI::ShowSigninScreen(SigninScreenHandlerDelegate* delegate) {
  signin_screen_handler_->SetDelegate(delegate);
  signin_screen_handler_->Show(core_handler_->show_oobe_ui());
}

void OobeUI::ResetSigninScreenHandlerDelegate() {
  signin_screen_handler_->SetDelegate(NULL);
}

void OobeUI::OnLoginPromptVisible() {
  user_image_screen_actor_->CheckCameraPresence();
}

}  // namespace chromeos
