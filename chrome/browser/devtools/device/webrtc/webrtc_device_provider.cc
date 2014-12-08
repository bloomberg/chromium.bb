// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/local_discovery/cloud_device_list.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
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

using BrowserInfo = AndroidDeviceManager::BrowserInfo;
using BrowserInfoList = std::vector<BrowserInfo>;
using DeviceInfo = AndroidDeviceManager::DeviceInfo;
using SerialList = std::vector<std::string>;
using content::BrowserThread;
using content::NotificationDetails;
using content::NotificationObserver;
using content::NotificationRegistrar;
using content::NotificationSource;
using content::WebContents;
using content::WebUIDataSource;
using content::WebUIMessageHandler;
using local_discovery::CloudDeviceList;
using local_discovery::CloudDeviceListDelegate;
using local_discovery::GCDApiFlow;

namespace {

const char kBackgroundWorkerURL[] =
    "chrome://webrtc-device-provider/background_worker.html";
const char kSerial[] = "clouddevices";
const char kPseudoDeviceName[] = "Remote browsers";

}  // namespace

// Lives on the UI thread.
class WebRTCDeviceProvider::DevToolsBridgeClient :
    private NotificationObserver,
    private IdentityProvider::Observer,
    private CloudDeviceListDelegate {
 public:
  static base::WeakPtr<DevToolsBridgeClient> Create(
      Profile* profile,
      SigninManagerBase* signin_manager,
      ProfileOAuth2TokenService* token_service);

  void DeleteSelf();

  static SerialList GetDevices(base::WeakPtr<DevToolsBridgeClient> weak_ptr);
  static DeviceInfo GetDeviceInfo(base::WeakPtr<DevToolsBridgeClient> weak_ptr,
                                  const std::string& serial);

 private:
  DevToolsBridgeClient(Profile* profile,
                       SigninManagerBase* signin_manager,
                       ProfileOAuth2TokenService* token_service);

  ~DevToolsBridgeClient();

  void CreateBackgroundWorker();
  void UpdateBrowserList();

  // Implementation of IdentityProvider::Observer.
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // Implementation of NotificationObserver.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // CloudDeviceListDelegate implementation.
  void OnDeviceListReady(
      const CloudDeviceListDelegate::DeviceList& devices) override;
  void OnDeviceListUnavailable() override;

  Profile* const profile_;
  ProfileIdentityProvider identity_provider_;
  NotificationRegistrar registrar_;
  scoped_ptr<WebContents> background_worker_;
  scoped_ptr<GCDApiFlow> browser_list_request_;
  BrowserInfoList browsers_;
  bool browser_list_updating_;
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
      browser_list_updating_(false),
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

void WebRTCDeviceProvider::DevToolsBridgeClient::UpdateBrowserList() {
  if (browser_list_updating_ || !browser_list_request_.get())
    return;
  browser_list_updating_ = true;

  browser_list_request_->Start(make_scoped_ptr(new CloudDeviceList(this)));
}

// static
SerialList WebRTCDeviceProvider::DevToolsBridgeClient::GetDevices(
    base::WeakPtr<DevToolsBridgeClient> weak_ptr) {
  SerialList result;
  if (auto* ptr = weak_ptr.get()) {
    if (ptr->background_worker_.get())
      result.push_back(kSerial);

    ptr->UpdateBrowserList();
  }
  return result;
}

// static
DeviceInfo WebRTCDeviceProvider::DevToolsBridgeClient::GetDeviceInfo(
    base::WeakPtr<DevToolsBridgeClient> weak_self,
    const std::string& serial) {
  DeviceInfo result;
  if (auto* self = weak_self.get()) {
    result.connected = !!self->background_worker_.get();
    result.model = kPseudoDeviceName;
    result.browser_info = self->browsers_;
  }
  return result;
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

  browser_list_request_ =
      GCDApiFlow::Create(profile_->GetRequestContext(),
                         identity_provider_.GetTokenService(),
                         identity_provider_.GetActiveAccountId());
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
  browser_list_request_.reset();
  BrowserInfoList().swap(browsers_);
}

void WebRTCDeviceProvider::DevToolsBridgeClient::OnDeviceListReady(
    const CloudDeviceListDelegate::DeviceList& devices) {
  browser_list_updating_ = false;

  // TODO(serya): Not all devices are browsers. Filter them out.
  BrowserInfoList browsers;
  browsers.reserve(devices.size());
  for (const auto& device : devices) {
    BrowserInfo browser;
    browser.type = BrowserInfo::kTypeChrome;
    browser.display_name = device.display_name;
    browsers.push_back(browser);
  }

  browsers_.swap(browsers);
}

void WebRTCDeviceProvider::DevToolsBridgeClient::OnDeviceListUnavailable() {
  browser_list_updating_ = false;

  BrowserInfoList().swap(browsers_);
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
  // TODO(serya): Implement
  scoped_ptr<net::StreamSocket> socket;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, net::ERR_FAILED, base::Passed(&socket)));
}
