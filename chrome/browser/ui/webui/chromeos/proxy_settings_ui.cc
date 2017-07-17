// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

class ProxySettingsHTMLSource : public content::URLDataSource {
 public:
  explicit ProxySettingsHTMLSource(base::DictionaryValue* localized_strings);

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string&) const override {
    return "text/html";
  }
  bool ShouldAddContentSecurityPolicy() const override { return false; }
  bool AllowCaching() const override {
    // Should not be cached to reflect dynamically-generated contents that
    // may depend on current settings.
    return false;
  }

 protected:
  ~ProxySettingsHTMLSource() override {}

 private:
  std::unique_ptr<base::DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsHTMLSource);
};

ProxySettingsHTMLSource::ProxySettingsHTMLSource(
    base::DictionaryValue* localized_strings)
    : localized_strings_(localized_strings) {
}

std::string ProxySettingsHTMLSource::GetSource() const {
  return chrome::kChromeUIProxySettingsHost;
}

void ProxySettingsHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings_.get());
  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PROXY_SETTINGS_HTML));
  std::string full_html = webui::GetI18nTemplateHtml(
      html, localized_strings_.get());

  callback.Run(base::RefCountedString::TakeString(&full_html));
}

}  // namespace

namespace chromeos {

ProxySettingsUI::ProxySettingsUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), initialized_handlers_(false) {
  // |localized_strings| will be owned by ProxySettingsHTMLSource.
  base::DictionaryValue* localized_strings = new base::DictionaryValue();

  auto core_handler = base::MakeUnique<options::CoreChromeOSOptionsHandler>();
  core_handler_ = core_handler.get();
  web_ui->AddMessageHandler(std::move(core_handler));
  core_handler_->set_handlers_host(this);
  core_handler_->GetLocalizedValues(localized_strings);

  auto proxy_handler = base::MakeUnique<options::ProxyHandler>();
  proxy_handler_ = proxy_handler.get();
  web_ui->AddMessageHandler(std::move(proxy_handler));
  proxy_handler_->GetLocalizedValues(localized_strings);

  internet_options_strings::RegisterLocalizedStrings(localized_strings);
  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  localized_strings->SetString("highlightStrength",
                               keyboard_driven_oobe ? "strong" : "normal");

  ProxySettingsHTMLSource* source =
      new ProxySettingsHTMLSource(localized_strings);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, source);
}

ProxySettingsUI::~ProxySettingsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them.
  core_handler_->Uninitialize();
  proxy_handler_->Uninitialize();
}

void ProxySettingsUI::InitializeHandlers() {
  // A new web page DOM has been brought up in an existing renderer, causing
  // this method to be called twice. In that case, don't initialize the handlers
  // again. Compare with options_ui.cc.
  if (!initialized_handlers_) {
    core_handler_->InitializeHandler();
    proxy_handler_->InitializeHandler();
    initialized_handlers_ = true;
  }
  core_handler_->InitializePage();
  proxy_handler_->InitializePage();
}

}  // namespace chromeos
