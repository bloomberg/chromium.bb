// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/webrtc_device_provider_resources_map.h"
#include "ui/base/page_transition_types.h"

const char kBackgroundWorkerURL[] =
    "chrome://webrtc-device-provider/background_worker.html";

namespace {

class MessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MessageHandler(WebRTCDeviceProvider* owner);

  void RegisterMessages() override;

 private:
  void HandleLoaded(const base::ListValue* args);

  WebRTCDeviceProvider* const owner_;
};

// MessageHandler -------------------------------------------------------------

MessageHandler::MessageHandler(
    WebRTCDeviceProvider* owner) : owner_(owner) {
}

void MessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded",
      base::Bind(&MessageHandler::HandleLoaded, base::Unretained(this)));
}

void MessageHandler::HandleLoaded(
    const base::ListValue* args) {
  if (!owner_)
    return;
  // TODO(serya): implement
}

}  // namespace

// WebRTCDeviceProvider::WebUI ------------------------------------------------

WebRTCDeviceProvider::WebUI::WebUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIWebRTCDeviceProviderHost);

  for (size_t i = 0; i < kWebrtcDeviceProviderResourcesSize; i++) {
    source->AddResourcePath(kWebrtcDeviceProviderResources[i].name,
                            kWebrtcDeviceProviderResources[i].value);
  }

  // Sets a stub message handler. If web contents was created by
  // WebRTCDeviceProvider message callbacks will be overridden by
  // a real implementation.
  web_ui->AddMessageHandler(new MessageHandler(nullptr));

  content::WebUIDataSource::Add(profile, source);
}

WebRTCDeviceProvider::WebUI::~WebUI() {
}

// WebRTCDeviceProvider -------------------------------------------------------

WebRTCDeviceProvider::WebRTCDeviceProvider(content::BrowserContext* context) {
  background_worker_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(context)));

  // TODO(serya): Make sure background_worker_ destructed before profile.
  GURL url(kBackgroundWorkerURL);

  DCHECK_EQ(chrome::kChromeUIWebRTCDeviceProviderHost, url.host());

  background_worker_->GetController().LoadURL(
      url,
      content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

  background_worker_->GetWebUI()->AddMessageHandler(
      new MessageHandler(this));
}

WebRTCDeviceProvider::~WebRTCDeviceProvider() {
}

void WebRTCDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  // TODO(serya): Implement
}

void WebRTCDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                          const DeviceInfoCallback& callback) {
  // TODO(serya): Implement
}

void WebRTCDeviceProvider::OpenSocket(const std::string& serial,
                                     const std::string& socket_name,
                                     const SocketCallback& callback) {
  // TODO(serya): Implement
}
