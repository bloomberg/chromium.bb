// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"

#include "base/json/json_writer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/local_discovery/cloud_device_list.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/webrtc_device_provider_resources_map.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
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
using content::WebContentsObserver;
using content::WebContentsUserData;
using content::WebUIDataSource;
using content::WebUIMessageHandler;
using local_discovery::CloudDeviceList;
using local_discovery::CloudDeviceListDelegate;
using local_discovery::GCDApiFlow;
using local_discovery::GCDApiFlowRequest;

namespace {

const char kBackgroundWorkerURL[] =
    "chrome://webrtc-device-provider/background_worker.html";
const char kSerial[] = "clouddevices";
const char kPseudoDeviceName[] = "Remote browsers";
const char kContentTypeJSON[] = "application/json";
const char kCommandTimeoutMs[] = "20000";
const char kDeviceIdPrefix[] = "device-id:";

class SendCommandRequest : public GCDApiFlowRequest {
 public:
  class Delegate {
   public:
    // It's safe to destroy SendCommandRequest in these methods.
    virtual void OnCommandSucceeded(const base::DictionaryValue& value) = 0;
    virtual void OnCommandFailed() = 0;

   protected:
    ~Delegate() {}
  };

  SendCommandRequest(const base::DictionaryValue* request,
                     Delegate* delegate);

  // Implementation of GCDApiFlowRequest.
  net::URLFetcher::RequestType GetRequestType() override;
  void GetUploadData(std::string* upload_type,
                     std::string* upload_data) override;
  void OnGCDAPIFlowError(GCDApiFlow::Status status) override;
  void OnGCDAPIFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;

 private:
  std::string upload_data_;
  Delegate* const delegate_;
};

class BackgroundWorkerUserData
    : public WebContentsUserData<BackgroundWorkerUserData> {
 public:
  DevToolsBridgeClient* client() const { return client_; }
  void SetClient(DevToolsBridgeClient* client) { client_ = client; }

 private:
  friend WebContentsUserData<BackgroundWorkerUserData>;

  explicit BackgroundWorkerUserData(WebContents* contents)
      : client_(nullptr) {
  }

  DevToolsBridgeClient* client_;
};

// SendCommandRequest ----------------------------------------------------------

SendCommandRequest::SendCommandRequest(const base::DictionaryValue* request,
                                       Delegate* delegate)
    : delegate_(delegate) {
  base::JSONWriter::Write(request, &upload_data_);
  DCHECK(delegate_);
}

net::URLFetcher::RequestType SendCommandRequest::GetRequestType() {
  return net::URLFetcher::POST;
}

void SendCommandRequest::GetUploadData(std::string* upload_type,
                                       std::string* upload_data) {
  *upload_type = kContentTypeJSON;
  *upload_data = upload_data_;
}

void SendCommandRequest::OnGCDAPIFlowError(GCDApiFlow::Status status) {
  delegate_->OnCommandFailed();
}

void SendCommandRequest::OnGCDAPIFlowComplete(
    const base::DictionaryValue& value) {
  delegate_->OnCommandSucceeded(value);
}

GURL SendCommandRequest::GetURL() {
  GURL url = cloud_devices::GetCloudDevicesRelativeURL("commands");
  url = net::AppendQueryParameter(url, "expireInMs", kCommandTimeoutMs);
  url = net::AppendQueryParameter(url, "responseAwaitMs", kCommandTimeoutMs);
  return url;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BackgroundWorkerUserData);

// Lives on the UI thread.
// TODO(serya): Move to a separate file.
class DevToolsBridgeClient final :
    private NotificationObserver,
    private IdentityProvider::Observer,
    private WebContentsObserver,
    private SendCommandRequest::Delegate,
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
  void StartSessionIfNeeded(const std::string& socket_name);

  static DevToolsBridgeClient* FromWebContents(WebContents* web_contents);
  void RegisterMessageHandlers(content::WebUI* web_ui);

 private:
  DevToolsBridgeClient(Profile* profile,
                       SigninManagerBase* signin_manager,
                       ProfileOAuth2TokenService* token_service);

  ~DevToolsBridgeClient();

  bool IsAuthenticated();

  void CreateBackgroundWorker();
  scoped_ptr<GCDApiFlow> CreateGCDApiFlow();
  void UpdateBrowserList();

  void HandleSendCommand(const base::ListValue* args);

  // Implementation of IdentityProvider::Observer.
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // Implementation of WebContentsObserver.
  void DocumentOnLoadCompletedInMainFrame() override;

  // Implementation of NotificationObserver.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Implementation of SendCommandRequest::Delegate.
  void OnCommandSucceeded(const base::DictionaryValue& response) override;
  void OnCommandFailed() override;

  // Implementation of CloudDeviceListDelegate.
  void OnDeviceListReady(
      const CloudDeviceListDelegate::DeviceList& devices) override;
  void OnDeviceListUnavailable() override;

  Profile* const profile_;
  ProfileIdentityProvider identity_provider_;
  NotificationRegistrar registrar_;
  scoped_ptr<WebContents> background_worker_;
  scoped_ptr<GCDApiFlow> browser_list_request_;
  scoped_ptr<GCDApiFlow> send_command_request_;
  BrowserInfoList browsers_;
  bool worker_is_loaded_;
  base::WeakPtrFactory<DevToolsBridgeClient> weak_factory_;
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

  auto client =
      DevToolsBridgeClient::FromWebContents(web_ui->GetWebContents());
  if (client)
    client->RegisterMessageHandlers(web_ui);

  WebUIDataSource::Add(profile, source);
}

WebRTCDeviceProvider::WebUI::~WebUI() {
}

// DevToolsBridgeClient --------------------------------------------------------

// static
base::WeakPtr<DevToolsBridgeClient> DevToolsBridgeClient::Create(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto instance = new DevToolsBridgeClient(
     profile, signin_manager, token_service);
  return instance->weak_factory_.GetWeakPtr();
}

DevToolsBridgeClient::DevToolsBridgeClient(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : WebContentsObserver(),
      profile_(profile),
      identity_provider_(signin_manager, token_service, nullptr),
      worker_is_loaded_(false),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  identity_provider_.AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));

  if (IsAuthenticated())
    CreateBackgroundWorker();
}

DevToolsBridgeClient::~DevToolsBridgeClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  identity_provider_.RemoveObserver(this);
}

void DevToolsBridgeClient::DeleteSelf() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete this;
}

void DevToolsBridgeClient::UpdateBrowserList() {
  if (!IsAuthenticated() || browser_list_request_.get())
    return;
  browser_list_request_ = CreateGCDApiFlow();
  browser_list_request_->Start(make_scoped_ptr(new CloudDeviceList(this)));
}

void DevToolsBridgeClient::StartSessionIfNeeded(
    const std::string& socket_name) {
  if (!background_worker_.get() ||
      !background_worker_->GetWebUI() ||
      !worker_is_loaded_) {
    return;
  }

  const size_t kPrefixLength = sizeof(kDeviceIdPrefix) - 1;
  if (socket_name.substr(0, kPrefixLength) != kDeviceIdPrefix)
    return;

  std::string browser_id = socket_name.substr(kPrefixLength);
  background_worker_->GetWebUI()->CallJavascriptFunction(
      "WebRTCDeviceProvider.instance.startSessionIfNeeded",
      base::StringValue(browser_id));
}

// static
DevToolsBridgeClient* DevToolsBridgeClient::FromWebContents(
    WebContents* web_contents) {
  auto user_data = BackgroundWorkerUserData::FromWebContents(web_contents);
  return user_data ? user_data->client() : nullptr;
}

void DevToolsBridgeClient::RegisterMessageHandlers(
    content::WebUI* web_ui) {
  web_ui->RegisterMessageCallback(
      "sendCommand",
      base::Bind(&DevToolsBridgeClient::HandleSendCommand,
                 base::Unretained(this)));
}

bool DevToolsBridgeClient::IsAuthenticated() {
  return !identity_provider_.GetActiveAccountId().empty();
}

void DevToolsBridgeClient::HandleSendCommand(
    const base::ListValue* args) {
  if (args->GetSize() != 1)
    return;

  const base::DictionaryValue* command_value;
  if (!args->GetDictionary(0, &command_value))
    return;

  send_command_request_ = CreateGCDApiFlow();
  send_command_request_->Start(make_scoped_ptr(
      new SendCommandRequest(command_value, this)));
}

scoped_ptr<GCDApiFlow> DevToolsBridgeClient::CreateGCDApiFlow() {
  DCHECK(IsAuthenticated());
  return GCDApiFlow::Create(profile_->GetRequestContext(),
                            identity_provider_.GetTokenService(),
                            identity_provider_.GetActiveAccountId());
}

// static
SerialList DevToolsBridgeClient::GetDevices(
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
DeviceInfo DevToolsBridgeClient::GetDeviceInfo(
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

void DevToolsBridgeClient::CreateBackgroundWorker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  background_worker_.reset(
      WebContents::Create(WebContents::CreateParams(profile_)));

  BackgroundWorkerUserData::CreateForWebContents(background_worker_.get());
  BackgroundWorkerUserData::FromWebContents(
      background_worker_.get())->SetClient(this);
  WebContentsObserver::Observe(background_worker_.get());

  GURL url(kBackgroundWorkerURL);
  DCHECK_EQ(chrome::kChromeUIWebRTCDeviceProviderHost, url.host());

  background_worker_->GetController().LoadURL(
      url,
      content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void DevToolsBridgeClient::DocumentOnLoadCompletedInMainFrame() {
  worker_is_loaded_ = true;
}

void DevToolsBridgeClient::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  delete this;
}

void DevToolsBridgeClient::OnActiveAccountLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CreateBackgroundWorker();
}

void DevToolsBridgeClient::OnActiveAccountLogout() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  background_worker_.reset();
  browser_list_request_.reset();
  send_command_request_.reset();
  BrowserInfoList().swap(browsers_);
  worker_is_loaded_ = false;
}

void DevToolsBridgeClient::OnCommandSucceeded(
    const base::DictionaryValue& response) {
  if (background_worker_.get() && background_worker_->GetWebUI()) {
    background_worker_->GetWebUI()->CallJavascriptFunction(
        "WebRTCDeviceProvider.instance.handleCommandSuccess", response);
  }
  send_command_request_.reset();
}

void DevToolsBridgeClient::OnCommandFailed() {
  if (background_worker_.get() && background_worker_->GetWebUI()) {
    background_worker_->GetWebUI()->CallJavascriptFunction(
        "WebRTCDeviceProvider.instance.handleCommandFailure");
  }
  send_command_request_.reset();
}

void DevToolsBridgeClient::OnDeviceListReady(
    const CloudDeviceListDelegate::DeviceList& devices) {
  // TODO(serya): Not all devices are browsers. Filter them out.
  BrowserInfoList browsers;
  browsers.reserve(devices.size());
  for (const auto& device : devices) {
    BrowserInfo browser;
    browser.type = BrowserInfo::kTypeChrome;
    browser.display_name = device.display_name;
    // Make virtual socket name of device ID to know which remote
    // instance we should connect to.
    browser.socket_name = kDeviceIdPrefix + device.id;
    browsers.push_back(browser);
  }

  browsers_.swap(browsers);
  browser_list_request_.reset();
}

void DevToolsBridgeClient::OnDeviceListUnavailable() {
  BrowserInfoList().swap(browsers_);
  browser_list_request_.reset();
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
