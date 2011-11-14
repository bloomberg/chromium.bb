// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class ProxySettingsHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit ProxySettingsHTMLSource(DictionaryValue* localized_strings);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsHTMLSource);
};

ProxySettingsHTMLSource::ProxySettingsHTMLSource(
    DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUIProxySettingsHost, MessageLoop::current()),
      localized_strings_(localized_strings) {
}

void ProxySettingsHTMLSource::StartDataRequest(const std::string& path,
                                               bool is_incognito,
                                               int request_id) {
  SetFontAndTextDirection(localized_strings_.get());

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PROXY_SETTINGS_HTML));
  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, localized_strings_.get());

  SendResponse(request_id, base::RefCountedString::TakeString(&full_html));
}

}  // namespace

namespace chromeos {

ProxySettingsUI::ProxySettingsUI(TabContents* contents)
    : ChromeWebUI(contents),
      proxy_handler_(new ProxyHandler()) {
  // |localized_strings| will be owned by ProxySettingsHTMLSource.
  DictionaryValue* localized_strings = new DictionaryValue();

  CoreChromeOSOptionsHandler* core_handler = new CoreChromeOSOptionsHandler();
  core_handler->set_handlers_host(this);
  core_handler->GetLocalizedValues(localized_strings);
  AddMessageHandler(core_handler->Attach(this));

  proxy_handler_->GetLocalizedValues(localized_strings);
  AddMessageHandler(proxy_handler_->Attach(this));

  ProxySettingsHTMLSource* source =
      new ProxySettingsHTMLSource(localized_strings);
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(source);
}

ProxySettingsUI::~ProxySettingsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them. Skip over the generic handler.
  for (std::vector<WebUIMessageHandler*>::iterator iter = handlers_.begin() + 1;
       iter != handlers_.end();
       ++iter) {
    static_cast<OptionsPageUIHandler*>(*iter)->Uninitialize();
  }
  proxy_handler_ = NULL;  // Weak ptr that is owned by base class, nullify it.
}

void ProxySettingsUI::InitializeHandlers() {
  std::vector<WebUIMessageHandler*>::iterator iter;
  // Skip over the generic handler.
  for (iter = handlers_.begin() + 1; iter != handlers_.end(); ++iter) {
    (static_cast<OptionsPageUIHandler*>(*iter))->Initialize();
  }
  PrefProxyConfigTracker* proxy_tracker = GetProfile()->GetProxyConfigTracker();
  proxy_tracker->UIMakeActiveNetworkCurrent();
  std::string network_name;
  proxy_tracker->UIGetCurrentNetworkName(&network_name);
  proxy_handler_->SetNetworkName(network_name);
}

}  // namespace chromeos
