// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
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
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, localized_strings_.get());

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

}  // namespace

namespace chromeos {

ProxySettingsUI::ProxySettingsUI(TabContents* contents) : WebUI(contents) {
  // |localized_strings| will be owned by ProxySettingsHTMLSource.
  DictionaryValue* localized_strings = new DictionaryValue();

  CoreChromeOSOptionsHandler* core_handler = new CoreChromeOSOptionsHandler();
  core_handler->set_handlers_host(this);
  core_handler->GetLocalizedValues(localized_strings);
  AddMessageHandler(core_handler->Attach(this));

  OptionsPageUIHandler* proxy_handler = new ProxyHandler();
  proxy_handler->GetLocalizedValues(localized_strings);
  AddMessageHandler(proxy_handler->Attach(this));

  ProxySettingsHTMLSource* source =
      new ProxySettingsHTMLSource(localized_strings);
  contents->profile()->GetChromeURLDataManager()->AddDataSource(source);
}

ProxySettingsUI::~ProxySettingsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them. Skip over the generic handler.
  for (std::vector<WebUIMessageHandler*>::iterator iter = handlers_.begin() + 1;
       iter != handlers_.end();
       ++iter) {
    static_cast<OptionsPageUIHandler*>(*iter)->Uninitialize();
  }
}

void ProxySettingsUI::InitializeHandlers() {
  std::vector<WebUIMessageHandler*>::iterator iter;
  // Skip over the generic handler.
  for (iter = handlers_.begin() + 1; iter != handlers_.end(); ++iter) {
    (static_cast<OptionsPageUIHandler*>(*iter))->Initialize();
  }
}

}  // namespace chromeos
