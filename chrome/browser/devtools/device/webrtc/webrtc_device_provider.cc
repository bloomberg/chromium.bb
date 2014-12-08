// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/webrtc_device_provider_resources_map.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "ui/base/page_transition_types.h"

using content::BrowserThread;
using content::NotificationDetails;
using content::NotificationObserver;
using content::NotificationRegistrar;
using content::NotificationSource;
using content::WebContents;
using content::WebUIDataSource;
using content::WebUIMessageHandler;

namespace {

const char kBackgroundWorkerURL[] =
    "chrome://webrtc-device-provider/background_worker.html";

}  // namespace

// Lives on the UI thread.
class WebRTCDeviceProvider::DevToolsBridgeClient :
    private NotificationObserver,
    private IdentityProvider::Observer {
 public:
  static base::WeakPtr<DevToolsBridgeClient> Create(
      Profile* profile,
      SigninManagerBase* signin_manager,
      ProfileOAuth2TokenService* token_service);

  void DeleteSelf();

 private:
  DevToolsBridgeClient(Profile* profile,
                       SigninManagerBase* signin_manager,
                       ProfileOAuth2TokenService* token_service);

  ~DevToolsBridgeClient();

  void CreateBackgroundWorker();

  // Implementation of IdentityProvider::Observer.
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // Implementation of NotificationObserver.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  Profile* const profile_;
  ProfileIdentityProvider identity_provider_;
  NotificationRegistrar registrar_;
  scoped_ptr<WebContents> background_worker_;
  base::WeakPtrFactory<DevToolsBridgeClient> weak_factory_;
};

class WebRTCDeviceProvider::MessageHandler : public WebUIMessageHandler {
 public:
  explicit MessageHandler(DevToolsBridgeClient* owner);

  void RegisterMessages() override;

 private:
  void HandleLoaded(const base::ListValue* args);

  DevToolsBridgeClient* const owner_;
};

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

  // Sets a stub message handler. If web contents was created by
  // WebRTCDeviceProvider message callbacks will be overridden by
  // a real implementation.
  web_ui->AddMessageHandler(new MessageHandler(nullptr));

  WebUIDataSource::Add(profile, source);
}

WebRTCDeviceProvider::WebUI::~WebUI() {
}

// WebRTCDeviceProvider::DevToolsBridgeClient ----------------------------------

// static
base::WeakPtr<WebRTCDeviceProvider::DevToolsBridgeClient>
WebRTCDeviceProvider::DevToolsBridgeClient::Create(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto instance = new DevToolsBridgeClient(
     profile, signin_manager, token_service);
  return instance->weak_factory_.GetWeakPtr();
}

WebRTCDeviceProvider::DevToolsBridgeClient::DevToolsBridgeClient(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : profile_(profile),
      identity_provider_(signin_manager, token_service, nullptr),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  identity_provider_.AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));

  if (!identity_provider_.GetActiveAccountId().empty())
    CreateBackgroundWorker();
}

WebRTCDeviceProvider::DevToolsBridgeClient::~DevToolsBridgeClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  identity_provider_.RemoveObserver(this);
}

void WebRTCDeviceProvider::DevToolsBridgeClient::DeleteSelf() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete this;
}

void WebRTCDeviceProvider::DevToolsBridgeClient::CreateBackgroundWorker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  background_worker_.reset(
      WebContents::Create(WebContents::CreateParams(profile_)));

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

void WebRTCDeviceProvider::DevToolsBridgeClient::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  delete this;
}

void WebRTCDeviceProvider::DevToolsBridgeClient::OnActiveAccountLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CreateBackgroundWorker();
}

void WebRTCDeviceProvider::DevToolsBridgeClient::OnActiveAccountLogout() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  background_worker_.reset();
}

// WebRTCDeviceProvider::MessageHandler ----------------------------------------

WebRTCDeviceProvider::MessageHandler::MessageHandler(
    DevToolsBridgeClient* owner) : owner_(owner) {
}

void WebRTCDeviceProvider::MessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded",
      base::Bind(&MessageHandler::HandleLoaded, base::Unretained(this)));
}

void WebRTCDeviceProvider::MessageHandler::HandleLoaded(
    const base::ListValue* args) {
  if (!owner_)
    return;
  // TODO(serya): implement
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
  // TODO(serya): Implement
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, std::vector<std::string>()));
}

void WebRTCDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                          const DeviceInfoCallback& callback) {
  // TODO(serya): Implement
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, AndroidDeviceManager::DeviceInfo()));
}

void WebRTCDeviceProvider::OpenSocket(const std::string& serial,
                                     const std::string& socket_name,
                                     const SocketCallback& callback) {
  // TODO(serya): Implement
  scoped_ptr<net::StreamSocket> socket;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, net::ERR_FAILED, base::Passed(&socket)));
}
