// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_client.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"

using content::BrowserThread;
using content::WebContents;

namespace {

const char kBackgroundWorkerURL[] =
    "chrome://webrtc-device-provider/background_worker.html";
const char kSerial[] = "webrtc";
const char kPseudoDeviceName[] = "Remote browsers";
const char kDeviceIdPrefix[] = "device-id:";

class BackgroundWorkerUserData
    : public content::WebContentsUserData<BackgroundWorkerUserData> {
 public:
  DevToolsBridgeClient* client() const { return client_; }
  void SetClient(DevToolsBridgeClient* client) { client_ = client; }

 private:
  friend WebContentsUserData<BackgroundWorkerUserData>;

  explicit BackgroundWorkerUserData(WebContents* contents) : client_(nullptr) {}

  DevToolsBridgeClient* client_;
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BackgroundWorkerUserData);

// DevToolsBridgeClient --------------------------------------------------------

// static
base::WeakPtr<DevToolsBridgeClient> DevToolsBridgeClient::Create(
    Profile* profile,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto instance =
      new DevToolsBridgeClient(profile, signin_manager, token_service);
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
  browser_list_request_->Start(
      make_scoped_ptr(new DevToolsBridgeInstancesRequest(this)));
}

void DevToolsBridgeClient::StartSessionIfNeeded(
    const std::string& socket_name) {
  if (!background_worker_.get() || !background_worker_->GetWebUI() ||
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

void DevToolsBridgeClient::RegisterMessageHandlers(content::WebUI* web_ui) {
  web_ui->RegisterMessageCallback(
      "sendCommand", base::Bind(&DevToolsBridgeClient::HandleSendCommand,
                                base::Unretained(this)));
}

bool DevToolsBridgeClient::IsAuthenticated() {
  return !identity_provider_.GetActiveAccountId().empty();
}

void DevToolsBridgeClient::HandleSendCommand(const base::ListValue* args) {
  if (args->GetSize() != 1)
    return;

  const base::DictionaryValue* command_value;
  if (!args->GetDictionary(0, &command_value))
    return;

  send_command_request_ = CreateGCDApiFlow();
  send_command_request_->Start(
      make_scoped_ptr(new SendCommandRequest(command_value, this)));
}

scoped_ptr<local_discovery::GCDApiFlow>
DevToolsBridgeClient::CreateGCDApiFlow() {
  DCHECK(IsAuthenticated());
  return local_discovery::GCDApiFlow::Create(
      profile_->GetRequestContext(), identity_provider_.GetTokenService(),
      identity_provider_.GetActiveAccountId());
}

// static
DevToolsBridgeClient::SerialList DevToolsBridgeClient::GetDevices(
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
DevToolsBridgeClient::DeviceInfo DevToolsBridgeClient::GetDeviceInfo(
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
  BackgroundWorkerUserData::FromWebContents(background_worker_.get())
      ->SetClient(this);
  WebContentsObserver::Observe(background_worker_.get());

  GURL url(kBackgroundWorkerURL);
  DCHECK_EQ(chrome::kChromeUIWebRTCDeviceProviderHost, url.host());

  background_worker_->GetController().LoadURL(url, content::Referrer(),
                                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                              std::string());
}

void DevToolsBridgeClient::DocumentOnLoadCompletedInMainFrame() {
  worker_is_loaded_ = true;
}

void DevToolsBridgeClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
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

void DevToolsBridgeClient::OnDevToolsBridgeInstancesRequestSucceeded(
    const DevToolsBridgeInstancesRequest::InstanceList& instances) {
  BrowserInfoList browsers;
  for (const auto& instance : instances) {
    BrowserInfo browser;
    browser.type = BrowserInfo::kTypeChrome;
    browser.display_name = instance.display_name;
    browser.socket_name = kDeviceIdPrefix + instance.id;
    browsers.push_back(browser);
  }
  browsers_.swap(browsers);

  browser_list_request_.reset();

  OnBrowserListUpdatedForTests();
}

void DevToolsBridgeClient::OnDevToolsBridgeInstancesRequestFailed() {
  // We keep the list of remote browsers even if the request failed.
  browser_list_request_.reset();
}
