// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_client.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/webrtc_device_provider_resources_map.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using content::BrowserThread;
using content::WebUIDataSource;

// WebRTCDeviceProvider::WebUI -------------------------------------------------

WebRTCDeviceProvider::WebUI::WebUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  WebUIDataSource* source = WebUIDataSource::Create(
      chrome::kChromeUIWebRTCDeviceProviderHost);

  for (size_t i = 0; i < kWebrtcDeviceProviderResourcesSize; i++) {
    source->AddResourcePath(kWebrtcDeviceProviderResources[i].name,
                            kWebrtcDeviceProviderResources[i].value);
  }

  auto client =
      DevToolsBridgeClient::FromWebContents(web_ui->GetWebContents());
  if (client)
    client->RegisterMessageHandlers(web_ui);

  WebUIDataSource::Add(profile, source);
}

WebRTCDeviceProvider::WebUI::~WebUI() {
}

// WebRTCDeviceProvider --------------------------------------------------------

WebRTCDeviceProvider::WebRTCDeviceProvider(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : client_(DevToolsBridgeClient::Create(
          profile, signin_manager, token_service)) {
}

WebRTCDeviceProvider::~WebRTCDeviceProvider() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsBridgeClient::DeleteSelf, client_));
}

void WebRTCDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsBridgeClient::GetDevices, client_),
      callback);
}

void WebRTCDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                           const DeviceInfoCallback& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsBridgeClient::GetDeviceInfo, client_, serial),
      callback);
}

void WebRTCDeviceProvider::OpenSocket(const std::string& serial,
                                     const std::string& socket_name,
                                     const SocketCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsBridgeClient::StartSessionIfNeeded,
                 client_, socket_name));
  // TODO(serya): Implement
  scoped_ptr<net::StreamSocket> socket;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, net::ERR_FAILED, base::Passed(&socket)));
}
