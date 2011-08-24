// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"

#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/enterprise_enrollment_screen_handler.h"
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

static const char kEnterpriseEnrollmentGaiaLoginPath[] = "gaialogin";

// Work around EnterpriseEnrollmentScreenHandler, which is suitable for the
// stand alone enterprise enrollment screen (views implementation).
class SingleEnterpriseEnrollmentScreenHandler
    : public EnterpriseEnrollmentScreenHandler {
 public:
  SingleEnterpriseEnrollmentScreenHandler();
  virtual ~SingleEnterpriseEnrollmentScreenHandler() {}

  // Overridden from EnterpriseEnrollmentScreenHandler:
  virtual void ShowConfirmationScreen() OVERRIDE;
  virtual void SetController(Controller* controller_) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handlers for WebUI messages.
  void HandleScreenReady(const ListValue* args);

  bool show_when_controller_is_set_;

  DISALLOW_COPY_AND_ASSIGN(SingleEnterpriseEnrollmentScreenHandler);
};

SingleEnterpriseEnrollmentScreenHandler::
    SingleEnterpriseEnrollmentScreenHandler()
    : show_when_controller_is_set_(false) {
}

void SingleEnterpriseEnrollmentScreenHandler::ShowConfirmationScreen() {
  RenderViewHost* render_view_host =
      web_ui_->tab_contents()->render_view_host();
  render_view_host->ExecuteJavascriptInWebFrame(
      string16(),
      UTF8ToUTF16("enterpriseEnrollment.showScreen('confirmation-screen');"));
  NotifyObservers(true);
}

void SingleEnterpriseEnrollmentScreenHandler::SetController(
    Controller* controller) {
  EnterpriseEnrollmentScreenHandler::SetController(controller);
  if (show_when_controller_is_set_) {
    show_when_controller_is_set_ = false;
    HandleScreenReady(NULL);
  }
}

void SingleEnterpriseEnrollmentScreenHandler::RegisterMessages() {
  EnterpriseEnrollmentScreenHandler::RegisterMessages();
  web_ui_->RegisterMessageCallback(
      "enrollmentScreenReady",
      NewCallback(this,
                  &SingleEnterpriseEnrollmentScreenHandler::HandleScreenReady));
}

void SingleEnterpriseEnrollmentScreenHandler::HandleScreenReady(
    const ListValue* value) {
  if (!controller_) {
    show_when_controller_is_set_ = true;
    return;
  }
  SetupGaiaStrings();
  web_ui_->CallJavascriptFunction("enterpriseEnrollment.showInitialScreen");
}

// A data source that provides the resources for the enterprise enrollment page.
// The enterprise enrollment page requests the HTML and other resources from
// this source.
class EnterpriseEnrollmentDataSource : public ChromeURLDataManager::DataSource {
 public:
  explicit EnterpriseEnrollmentDataSource(DictionaryValue* localized_strings);

  // DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  virtual ~EnterpriseEnrollmentDataSource();

  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentDataSource);
};

EnterpriseEnrollmentDataSource::EnterpriseEnrollmentDataSource(
    DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUIEnterpriseEnrollmentHost,
                 MessageLoop::current()),
      localized_strings_(localized_strings) {
}

void EnterpriseEnrollmentDataSource::StartDataRequest(const std::string& path,
                                                      bool is_off_the_record,
                                                      int request_id) {
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  std::string response;
  if (path.empty()) {
    static const base::StringPiece html(
        resource_bundle.GetRawDataResource(IDR_ENTERPRISE_ENROLLMENT_HTML));
    response =
        jstemplate_builder::GetI18nTemplateHtml(html, localized_strings_.get());
  } else if (path == kEnterpriseEnrollmentGaiaLoginPath) {
    static const base::StringPiece html(resource_bundle.GetRawDataResource(
        IDR_GAIA_LOGIN_HTML));
    response =
        jstemplate_builder::GetI18nTemplateHtml(html, localized_strings_.get());
  }

  SendResponse(request_id, base::RefCountedString::TakeString(&response));
}

std::string EnterpriseEnrollmentDataSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}

EnterpriseEnrollmentDataSource::~EnterpriseEnrollmentDataSource() {}

EnterpriseEnrollmentUI::EnterpriseEnrollmentUI(TabContents* contents)
    : ChromeWebUI(contents), handler_(NULL) {}

EnterpriseEnrollmentUI::~EnterpriseEnrollmentUI() {}

void EnterpriseEnrollmentUI::RenderViewCreated(
    RenderViewHost* render_view_host) {
  // Bail out early if web ui is disabled.
  if (!(bindings_ & BindingsPolicy::WEB_UI))
    return;

  handler_ = new SingleEnterpriseEnrollmentScreenHandler();
  AddMessageHandler(handler_->Attach(this));

  scoped_ptr<DictionaryValue> localized_strings(new DictionaryValue);
  handler_->GetLocalizedStrings(localized_strings.get());
  ChromeURLDataManager::DataSource::SetFontAndTextDirection(
      localized_strings.get());
  // Set up the data source, so the enrollment page can be loaded.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      new EnterpriseEnrollmentDataSource(localized_strings.release()));
}

EnterpriseEnrollmentScreenActor* EnterpriseEnrollmentUI::GetActor() {
  return handler_;
}

}  // namespace chromeos
