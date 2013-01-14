// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

class ProxySettingsHTMLSource : public content::URLDataSourceDelegate {
 public:
  explicit ProxySettingsHTMLSource(DictionaryValue* localized_strings);

  // content::URLDataSourceDelegate implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE {
    return "text/html";
  }

 protected:
  virtual ~ProxySettingsHTMLSource() {}

 private:
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsHTMLSource);
};

ProxySettingsHTMLSource::ProxySettingsHTMLSource(
    DictionaryValue* localized_strings)
    : localized_strings_(localized_strings) {
}

std::string ProxySettingsHTMLSource::GetSource() {
  return chrome::kChromeUIProxySettingsHost;
}

void ProxySettingsHTMLSource::StartDataRequest(const std::string& path,
                                               bool is_incognito,
                                               int request_id) {
  URLDataSource::SetFontAndTextDirection(localized_strings_.get());

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PROXY_SETTINGS_HTML));
  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, localized_strings_.get());

  url_data_source()->SendResponse(
      request_id, base::RefCountedString::TakeString(&full_html));
}

}  // namespace

namespace chromeos {

ProxySettingsUI::ProxySettingsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      proxy_handler_(new options::ProxyHandler()),
      core_handler_(new options::CoreChromeOSOptionsHandler()) {
  // |localized_strings| will be owned by ProxySettingsHTMLSource.
  DictionaryValue* localized_strings = new DictionaryValue();

  core_handler_->set_handlers_host(this);
  core_handler_->GetLocalizedValues(localized_strings);
  web_ui->AddMessageHandler(core_handler_);

  proxy_handler_->GetLocalizedValues(localized_strings);
  web_ui->AddMessageHandler(proxy_handler_);

  ProxySettingsHTMLSource* source =
      new ProxySettingsHTMLSource(localized_strings);
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

ProxySettingsUI::~ProxySettingsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them.
  core_handler_->Uninitialize();
  proxy_handler_->Uninitialize();
}

void ProxySettingsUI::InitializeHandlers() {
  core_handler_->InitializeHandler();
  proxy_handler_->InitializeHandler();
  core_handler_->InitializePage();
  proxy_handler_->InitializePage();
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefProxyConfigTracker* proxy_tracker = profile->GetProxyConfigTracker();
  proxy_tracker->UIMakeActiveNetworkCurrent();
  std::string network_name;
  proxy_tracker->UIGetCurrentNetworkName(&network_name);
}

}  // namespace chromeos
