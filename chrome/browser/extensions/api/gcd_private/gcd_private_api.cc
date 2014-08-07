// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/cloud_device_list.h"
#include "chrome/browser/local_discovery/cloud_print_printer_list.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/browser/local_discovery/privetv3_session.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "extensions/browser/event_router.h"
#include "net/base/net_util.h"

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
#include "chrome/browser/local_discovery/wifi/wifi_manager.h"
#endif

namespace extensions {

namespace gcd_private = api::gcd_private;

namespace {

const int kNumRequestsNeeded = 2;

const char kIDPrefixCloudPrinter[] = "cloudprint:";
const char kIDPrefixGcd[] = "gcd:";
const char kIDPrefixMdns[] = "mdns:";

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
const char kPrivatAPISetup[] = "/privet/v3/setup/start";
const char kPrivetKeyWifi[] = "wifi";
const char kPrivetKeyPassphrase[] = "passphrase";
const char kPrivetKeySSID[] = "ssid";
const char kPrivetKeyPassphraseDotted[] = "wifi.passphrase";
#endif  // ENABLE_WIFI_BOOTSTRAPPING

scoped_ptr<Event> MakeDeviceStateChangedEvent(
    const gcd_private::GCDDevice& device) {
  scoped_ptr<base::ListValue> params =
      gcd_private::OnDeviceStateChanged::Create(device);
  scoped_ptr<Event> event(
      new Event(gcd_private::OnDeviceStateChanged::kEventName, params.Pass()));
  return event.Pass();
}

scoped_ptr<Event> MakeDeviceRemovedEvent(const std::string& device) {
  scoped_ptr<base::ListValue> params =
      gcd_private::OnDeviceRemoved::Create(device);
  scoped_ptr<Event> event(
      new Event(gcd_private::OnDeviceRemoved::kEventName, params.Pass()));
  return event.Pass();
}

GcdPrivateAPI::GCDApiFlowFactoryForTests* g_gcd_api_flow_factory = NULL;

base::LazyInstance<BrowserContextKeyedAPIFactory<GcdPrivateAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

scoped_ptr<local_discovery::GCDApiFlow> MakeGCDApiFlow(Profile* profile) {
  if (g_gcd_api_flow_factory) {
    return g_gcd_api_flow_factory->CreateGCDApiFlow();
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (!token_service)
    return scoped_ptr<local_discovery::GCDApiFlow>();
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile);
  if (!signin_manager)
    return scoped_ptr<local_discovery::GCDApiFlow>();
  return local_discovery::GCDApiFlow::Create(
      profile->GetRequestContext(),
      token_service,
      signin_manager->GetAuthenticatedAccountId());
}

}  // namespace

class GcdPrivateSessionHolder;

class GcdPrivateAPIImpl : public EventRouter::Observer,
                          public local_discovery::PrivetDeviceLister::Delegate {
 public:
  typedef base::Callback<void(bool success)> SuccessCallback;

  typedef base::Callback<void(int session_id,
                              api::gcd_private::Status status,
                              const std::string& code,
                              api::gcd_private::ConfirmationType type)>
      ConfirmationCodeCallback;

  typedef base::Callback<void(api::gcd_private::Status status)>
      SessionEstablishedCallback;

  typedef base::Callback<void(api::gcd_private::Status status,
                              const base::DictionaryValue& response)>
      MessageResponseCallback;

  explicit GcdPrivateAPIImpl(content::BrowserContext* context);
  virtual ~GcdPrivateAPIImpl();

  static GcdPrivateAPIImpl* Get(content::BrowserContext* context);

  bool QueryForDevices();

  void EstablishSession(const std::string& ip_address,
                        int port,
                        ConfirmationCodeCallback callback);

  void ConfirmCode(int session_id, SessionEstablishedCallback callback);

  void SendMessage(int session_id,
                   const std::string& api,
                   const base::DictionaryValue& input,
                   MessageResponseCallback callback);

  void RequestWifiPassword(const std::string& ssid,
                           const SuccessCallback& callback);

  void RemoveSession(int session_id);

  scoped_ptr<base::ListValue> GetPrefetchedSSIDList();

 private:
  typedef std::map<std::string /* id_string */,
                   linked_ptr<api::gcd_private::GCDDevice> > GCDDeviceMap;

  typedef std::map<int /* session id*/, linked_ptr<GcdPrivateSessionHolder> >
      GCDSessionMap;

  typedef std::map<std::string /* ssid */, std::string /* password */>
      PasswordMap;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // local_discovery::PrivetDeviceLister implementation.
  virtual void DeviceChanged(
      bool added,
      const std::string& name,
      const local_discovery::DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;
  virtual void DeviceCacheFlushed() OVERRIDE;

  void SendMessageInternal(int session_id,
                           const std::string& api,
                           const base::DictionaryValue& input,
                           const MessageResponseCallback& callback);

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  void OnWifiPassword(const SuccessCallback& callback,
                      bool success,
                      const std::string& ssid,
                      const std::string& password);
  void StartWifiIfNotStarted();
#endif

  int num_device_listeners_;
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  scoped_ptr<local_discovery::PrivetDeviceLister> privet_device_lister_;
  GCDDeviceMap known_devices_;

  GCDSessionMap sessions_;
  int last_session_id_;

  content::BrowserContext* const browser_context_;

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  scoped_ptr<local_discovery::wifi::WifiManager> wifi_manager_;
  PasswordMap wifi_passwords_;
#endif
};

class GcdPrivateRequest : public local_discovery::PrivetV3Session::Request {
 public:
  GcdPrivateRequest(const std::string& api,
                    const base::DictionaryValue& input,
                    const GcdPrivateAPIImpl::MessageResponseCallback& callback,
                    GcdPrivateSessionHolder* session_holder);
  virtual ~GcdPrivateRequest();

  // local_discovery::PrivetV3Session::Request implementation.
  virtual std::string GetName() OVERRIDE;
  virtual const base::DictionaryValue& GetInput() OVERRIDE;
  virtual void OnError(
      local_discovery::PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(const base::DictionaryValue& value,
                            bool has_error) OVERRIDE;

 private:
  std::string api_;
  scoped_ptr<base::DictionaryValue> input_;
  GcdPrivateAPIImpl::MessageResponseCallback callback_;
  GcdPrivateSessionHolder* session_holder_;
};

class GcdPrivateSessionHolder
    : public local_discovery::PrivetV3Session::Delegate {
 public:
  typedef base::Callback<void(api::gcd_private::Status status,
                              const std::string& code,
                              api::gcd_private::ConfirmationType type)>
      ConfirmationCodeCallback;

  GcdPrivateSessionHolder(const std::string& ip_address,
                          int port,
                          net::URLRequestContextGetter* request_context);
  virtual ~GcdPrivateSessionHolder();

  void Start(const ConfirmationCodeCallback& callback);

  void ConfirmCode(
      const GcdPrivateAPIImpl::SessionEstablishedCallback& callback);

  void SendMessage(const std::string& api,
                   const base::DictionaryValue& input,
                   GcdPrivateAPIImpl::MessageResponseCallback callback);

  void DeleteRequest(GcdPrivateRequest* request);

 private:
  // local_discovery::PrivetV3Session::Delegate implementation.
  virtual void OnSetupConfirmationNeeded(
      const std::string& confirmation_code) OVERRIDE;
  virtual void OnSessionEstablished() OVERRIDE;
  virtual void OnCannotEstablishSession() OVERRIDE;

  scoped_ptr<local_discovery::PrivetHTTPClient> http_client_;
  scoped_ptr<local_discovery::PrivetV3Session> privet_session_;
  typedef ScopedVector<GcdPrivateRequest> RequestVector;
  RequestVector requests_;

  ConfirmationCodeCallback confirm_callback_;
  GcdPrivateAPIImpl::SessionEstablishedCallback session_established_callback_;
};

GcdPrivateAPIImpl::GcdPrivateAPIImpl(content::BrowserContext* context)
    : num_device_listeners_(0), last_session_id_(0), browser_context_(context) {
  DCHECK(browser_context_);
  if (EventRouter::Get(context)) {
    EventRouter::Get(context)
        ->RegisterObserver(this, gcd_private::OnDeviceStateChanged::kEventName);
    EventRouter::Get(context)
        ->RegisterObserver(this, gcd_private::OnDeviceRemoved::kEventName);
  }
}

GcdPrivateAPIImpl::~GcdPrivateAPIImpl() {
  if (EventRouter::Get(browser_context_)) {
    EventRouter::Get(browser_context_)->UnregisterObserver(this);
  }
}

void GcdPrivateAPIImpl::OnListenerAdded(const EventListenerInfo& details) {
  if (details.event_name == gcd_private::OnDeviceStateChanged::kEventName ||
      details.event_name == gcd_private::OnDeviceRemoved::kEventName) {
    num_device_listeners_++;

    if (num_device_listeners_ == 1) {
      service_discovery_client_ =
          local_discovery::ServiceDiscoverySharedClient::GetInstance();
      privet_device_lister_.reset(new local_discovery::PrivetDeviceListerImpl(
          service_discovery_client_.get(), this));
      privet_device_lister_->Start();
    }

    for (GCDDeviceMap::iterator i = known_devices_.begin();
         i != known_devices_.end();
         i++) {
      EventRouter::Get(browser_context_)->DispatchEventToExtension(
          details.extension_id, MakeDeviceStateChangedEvent(*i->second));
    }
  }
}

void GcdPrivateAPIImpl::OnListenerRemoved(const EventListenerInfo& details) {
  if (details.event_name == gcd_private::OnDeviceStateChanged::kEventName ||
      details.event_name == gcd_private::OnDeviceRemoved::kEventName) {
    num_device_listeners_--;

    if (num_device_listeners_ == 0) {
      privet_device_lister_.reset();
      service_discovery_client_ = NULL;
    }
  }
}

void GcdPrivateAPIImpl::DeviceChanged(
    bool added,
    const std::string& name,
    const local_discovery::DeviceDescription& description) {
  linked_ptr<gcd_private::GCDDevice> device(new gcd_private::GCDDevice);
  device->setup_type = gcd_private::SETUP_TYPE_MDNS;
  device->device_id = kIDPrefixMdns + name;
  device->device_type = description.type;
  device->device_name = description.name;
  device->device_description = description.description;
  if (!description.id.empty())
    device->cloud_id.reset(new std::string(description.id));

  known_devices_[device->device_id] = device;

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(MakeDeviceStateChangedEvent(*device));
}

void GcdPrivateAPIImpl::DeviceRemoved(const std::string& name) {
  GCDDeviceMap::iterator found = known_devices_.find(kIDPrefixMdns + name);
  linked_ptr<gcd_private::GCDDevice> device = found->second;
  known_devices_.erase(found);

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(MakeDeviceRemovedEvent(device->device_id));
}

void GcdPrivateAPIImpl::DeviceCacheFlushed() {
  for (GCDDeviceMap::iterator i = known_devices_.begin();
       i != known_devices_.end();
       i++) {
    EventRouter::Get(browser_context_)
        ->BroadcastEvent(MakeDeviceRemovedEvent(i->second->device_id));
  }

  known_devices_.clear();
}

// static
GcdPrivateAPIImpl* GcdPrivateAPIImpl::Get(content::BrowserContext* context) {
  GcdPrivateAPI* gcd_api =
      BrowserContextKeyedAPIFactory<GcdPrivateAPI>::Get(context);
  return gcd_api ? gcd_api->impl_.get() : NULL;
}

bool GcdPrivateAPIImpl::QueryForDevices() {
  if (!privet_device_lister_)
    return false;

  privet_device_lister_->DiscoverNewDevices(true);

  return true;
}

void GcdPrivateAPIImpl::EstablishSession(const std::string& ip_address,
                                         int port,
                                         ConfirmationCodeCallback callback) {
  int session_id = last_session_id_++;
  linked_ptr<GcdPrivateSessionHolder> session_handler(
      new GcdPrivateSessionHolder(
          ip_address, port, browser_context_->GetRequestContext()));
  sessions_[session_id] = session_handler;
  session_handler->Start(base::Bind(callback, session_id));
}

void GcdPrivateAPIImpl::ConfirmCode(int session_id,
                                    SessionEstablishedCallback callback) {
  GCDSessionMap::iterator found = sessions_.find(session_id);

  if (found == sessions_.end()) {
    callback.Run(gcd_private::STATUS_UNKNOWNSESSIONERROR);
    return;
  }

  found->second->ConfirmCode(callback);
}

void GcdPrivateAPIImpl::SendMessage(int session_id,
                                    const std::string& api,
                                    const base::DictionaryValue& input,
                                    MessageResponseCallback callback) {
  const base::DictionaryValue* input_actual = &input;
#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  scoped_ptr<base::DictionaryValue> input_cloned;

  if (api == kPrivatAPISetup) {
    const base::DictionaryValue* wifi = NULL;

    if (input.GetDictionary(kPrivetKeyWifi, &wifi)) {
      std::string ssid;

      if (!wifi->GetString(kPrivetKeySSID, &ssid)) {
        callback.Run(gcd_private::STATUS_SETUPPARSEERROR,
                     base::DictionaryValue());
        return;
      }

      if (!wifi->HasKey(kPrivetKeyPassphrase)) {
        // If the message is a setup message, has a wifi section, try sending
        // the passphrase.

        PasswordMap::iterator found = wifi_passwords_.find(ssid);
        if (found == wifi_passwords_.end()) {
          callback.Run(gcd_private::STATUS_WIFIPASSWORDERROR,
                       base::DictionaryValue());
          return;
        }

        input_cloned.reset(input.DeepCopy());
        input_cloned->SetString(kPrivetKeyPassphraseDotted, found->second);
        input_actual = input_cloned.get();
      }
    }
  }
#endif

  GCDSessionMap::iterator found = sessions_.find(session_id);

  if (found == sessions_.end()) {
    callback.Run(gcd_private::STATUS_UNKNOWNSESSIONERROR,
                 base::DictionaryValue());
    return;
  }

  found->second->SendMessage(api, *input_actual, callback);
}

void GcdPrivateAPIImpl::RequestWifiPassword(const std::string& ssid,
                                            const SuccessCallback& callback) {
#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  StartWifiIfNotStarted();
  wifi_manager_->RequestNetworkCredentials(
      ssid,
      base::Bind(&GcdPrivateAPIImpl::OnWifiPassword,
                 base::Unretained(this),
                 callback));
#else
  callback.Run(false);
#endif
}

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
void GcdPrivateAPIImpl::OnWifiPassword(const SuccessCallback& callback,
                                       bool success,
                                       const std::string& ssid,
                                       const std::string& password) {
  if (success) {
    wifi_passwords_[ssid] = password;
  }

  callback.Run(success);
}

void GcdPrivateAPIImpl::StartWifiIfNotStarted() {
  if (!wifi_manager_) {
    wifi_manager_ = local_discovery::wifi::WifiManager::Create();
    wifi_manager_->Start();
  }
}

#endif

void GcdPrivateAPIImpl::RemoveSession(int session_id) {
  sessions_.erase(session_id);
}

scoped_ptr<base::ListValue> GcdPrivateAPIImpl::GetPrefetchedSSIDList() {
  scoped_ptr<base::ListValue> retval(new base::ListValue);

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  for (PasswordMap::iterator i = wifi_passwords_.begin();
       i != wifi_passwords_.end();
       i++) {
    retval->AppendString(i->first);
  }
#endif

  return retval.Pass();
}

GcdPrivateRequest::GcdPrivateRequest(
    const std::string& api,
    const base::DictionaryValue& input,
    const GcdPrivateAPIImpl::MessageResponseCallback& callback,
    GcdPrivateSessionHolder* session_holder)
    : api_(api),
      input_(input.DeepCopy()),
      callback_(callback),
      session_holder_(session_holder) {
}

GcdPrivateRequest::~GcdPrivateRequest() {
}

std::string GcdPrivateRequest::GetName() {
  return api_;
}

const base::DictionaryValue& GcdPrivateRequest::GetInput() {
  return *input_;
}

void GcdPrivateRequest::OnError(
    local_discovery::PrivetURLFetcher::ErrorType error) {
  callback_.Run(gcd_private::STATUS_CONNECTIONERROR, base::DictionaryValue());

  session_holder_->DeleteRequest(this);
}

void GcdPrivateRequest::OnParsedJson(const base::DictionaryValue& value,
                                     bool has_error) {
  callback_.Run(gcd_private::STATUS_SUCCESS, value);

  session_holder_->DeleteRequest(this);
}

GcdPrivateSessionHolder::GcdPrivateSessionHolder(
    const std::string& ip_address,
    int port,
    net::URLRequestContextGetter* request_context) {
  std::string host_string;
  net::IPAddressNumber address_number;

  if (net::ParseIPLiteralToNumber(ip_address, &address_number) &&
      address_number.size() == net::kIPv6AddressSize) {
    host_string = base::StringPrintf("[%s]", ip_address.c_str());
  } else {
    host_string = ip_address;
  }

  http_client_.reset(new local_discovery::PrivetHTTPClientImpl(
      "", net::HostPortPair(host_string, port), request_context));
}

GcdPrivateSessionHolder::~GcdPrivateSessionHolder() {
}

void GcdPrivateSessionHolder::Start(const ConfirmationCodeCallback& callback) {
  confirm_callback_ = callback;

  privet_session_.reset(
      new local_discovery::PrivetV3Session(http_client_.Pass(), this));
  privet_session_->Start();
}

void GcdPrivateSessionHolder::ConfirmCode(
    const GcdPrivateAPIImpl::SessionEstablishedCallback& callback) {
  session_established_callback_ = callback;
  privet_session_->ConfirmCode();
}

void GcdPrivateSessionHolder::SendMessage(
    const std::string& api,
    const base::DictionaryValue& input,
    GcdPrivateAPIImpl::MessageResponseCallback callback) {
  GcdPrivateRequest* request =
      new GcdPrivateRequest(api, input, callback, this);
  requests_.push_back(request);
  privet_session_->StartRequest(request);
}

void GcdPrivateSessionHolder::DeleteRequest(GcdPrivateRequest* request) {
  // TODO(noamsml): Does this need to be optimized?
  for (RequestVector::iterator i = requests_.begin(); i != requests_.end();
       i++) {
    if (*i == request) {
      requests_.erase(i);
      break;
    }
  }
}

void GcdPrivateSessionHolder::OnSetupConfirmationNeeded(
    const std::string& confirmation_code) {
  confirm_callback_.Run(gcd_private::STATUS_SUCCESS,
                        confirmation_code,
                        gcd_private::CONFIRMATION_TYPE_DISPLAYCODE);

  confirm_callback_.Reset();
}

void GcdPrivateSessionHolder::OnSessionEstablished() {
  session_established_callback_.Run(gcd_private::STATUS_SUCCESS);

  session_established_callback_.Reset();
}

void GcdPrivateSessionHolder::OnCannotEstablishSession() {
  session_established_callback_.Run(gcd_private::STATUS_SESSIONERROR);

  session_established_callback_.Reset();
}

GcdPrivateAPI::GcdPrivateAPI(content::BrowserContext* context)
    : impl_(new GcdPrivateAPIImpl(context)) {
}

GcdPrivateAPI::~GcdPrivateAPI() {
}

// static
BrowserContextKeyedAPIFactory<GcdPrivateAPI>*
GcdPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
void GcdPrivateAPI::SetGCDApiFlowFactoryForTests(
    GCDApiFlowFactoryForTests* factory) {
  g_gcd_api_flow_factory = factory;
}

GcdPrivateGetCloudDeviceListFunction::GcdPrivateGetCloudDeviceListFunction() {
}

GcdPrivateGetCloudDeviceListFunction::~GcdPrivateGetCloudDeviceListFunction() {
}

bool GcdPrivateGetCloudDeviceListFunction::RunAsync() {
  requests_succeeded_ = 0;
  requests_failed_ = 0;

  printer_list_ = MakeGCDApiFlow(GetProfile());
  device_list_ = MakeGCDApiFlow(GetProfile());

  if (!printer_list_ || !device_list_)
    return false;

  // Balanced in CheckListingDone()
  AddRef();

  printer_list_->Start(make_scoped_ptr<local_discovery::GCDApiFlow::Request>(
      new local_discovery::CloudPrintPrinterList(this)));
  device_list_->Start(make_scoped_ptr<local_discovery::GCDApiFlow::Request>(
      new local_discovery::CloudDeviceList(this)));

  return true;
}

void GcdPrivateGetCloudDeviceListFunction::OnDeviceListReady(
    const DeviceList& devices) {
  requests_succeeded_++;

  devices_.insert(devices_.end(), devices.begin(), devices.end());

  CheckListingDone();
}

void GcdPrivateGetCloudDeviceListFunction::OnDeviceListUnavailable() {
  requests_failed_++;

  CheckListingDone();
}

void GcdPrivateGetCloudDeviceListFunction::CheckListingDone() {
  if (requests_failed_ + requests_succeeded_ != kNumRequestsNeeded)
    return;

  if (requests_succeeded_ == 0) {
    SendResponse(false);
    return;
  }

  std::vector<linked_ptr<gcd_private::GCDDevice> > devices;

  for (DeviceList::iterator i = devices_.begin(); i != devices_.end(); i++) {
    linked_ptr<gcd_private::GCDDevice> device(new gcd_private::GCDDevice);
    device->setup_type = gcd_private::SETUP_TYPE_CLOUD;
    if (i->type == local_discovery::kGCDTypePrinter) {
      device->device_id = kIDPrefixCloudPrinter + i->id;
    } else {
      device->device_id = kIDPrefixGcd + i->id;
    }

    device->cloud_id.reset(new std::string(i->id));
    device->device_type = i->type;
    device->device_name = i->display_name;
    device->device_description = i->description;

    devices.push_back(device);
  }

  results_ = gcd_private::GetCloudDeviceList::Results::Create(devices);

  SendResponse(true);
  Release();
}

GcdPrivateQueryForNewLocalDevicesFunction::
    GcdPrivateQueryForNewLocalDevicesFunction() {
}

GcdPrivateQueryForNewLocalDevicesFunction::
    ~GcdPrivateQueryForNewLocalDevicesFunction() {
}

bool GcdPrivateQueryForNewLocalDevicesFunction::RunSync() {
  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  if (!gcd_api->QueryForDevices()) {
    error_ =
        "You must first subscribe to onDeviceStateChanged or onDeviceRemoved "
        "notifications";
    return false;
  }

  return true;
}

GcdPrivatePrefetchWifiPasswordFunction::
    GcdPrivatePrefetchWifiPasswordFunction() {
}

GcdPrivatePrefetchWifiPasswordFunction::
    ~GcdPrivatePrefetchWifiPasswordFunction() {
}

bool GcdPrivatePrefetchWifiPasswordFunction::RunAsync() {
  scoped_ptr<gcd_private::PrefetchWifiPassword::Params> params =
      gcd_private::PrefetchWifiPassword::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->RequestWifiPassword(
      params->ssid,
      base::Bind(&GcdPrivatePrefetchWifiPasswordFunction::OnResponse, this));

  return true;
}

void GcdPrivatePrefetchWifiPasswordFunction::OnResponse(bool response) {
  scoped_ptr<base::FundamentalValue> response_value(
      new base::FundamentalValue(response));
  SetResult(response_value.release());
  SendResponse(true);
}

GcdPrivateEstablishSessionFunction::GcdPrivateEstablishSessionFunction() {
}

GcdPrivateEstablishSessionFunction::~GcdPrivateEstablishSessionFunction() {
}

bool GcdPrivateEstablishSessionFunction::RunAsync() {
  scoped_ptr<gcd_private::EstablishSession::Params> params =
      gcd_private::EstablishSession::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->EstablishSession(
      params->ip_address,
      params->port,
      base::Bind(&GcdPrivateEstablishSessionFunction::OnConfirmCodeCallback,
                 this));

  return true;
}

void GcdPrivateEstablishSessionFunction::OnConfirmCodeCallback(
    int session_id,
    gcd_private::Status status,
    const std::string& confirm_code,
    gcd_private::ConfirmationType confirmation_type) {
  results_ = gcd_private::EstablishSession::Results::Create(
      session_id, status, confirm_code, confirmation_type);
  SendResponse(true);
}

GcdPrivateConfirmCodeFunction::GcdPrivateConfirmCodeFunction() {
}

GcdPrivateConfirmCodeFunction::~GcdPrivateConfirmCodeFunction() {
}

bool GcdPrivateConfirmCodeFunction::RunAsync() {
  scoped_ptr<gcd_private::ConfirmCode::Params> params =
      gcd_private::ConfirmCode::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->ConfirmCode(
      params->session_id,
      base::Bind(&GcdPrivateConfirmCodeFunction::OnSessionEstablishedCallback,
                 this));

  return true;
}

void GcdPrivateConfirmCodeFunction::OnSessionEstablishedCallback(
    api::gcd_private::Status status) {
  results_ = gcd_private::ConfirmCode::Results::Create(status);
  SendResponse(true);
}

GcdPrivateSendMessageFunction::GcdPrivateSendMessageFunction() {
}

GcdPrivateSendMessageFunction::~GcdPrivateSendMessageFunction() {
}

bool GcdPrivateSendMessageFunction::RunAsync() {
  scoped_ptr<gcd_private::PassMessage::Params> params =
      gcd_private::PassMessage::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());


  gcd_api->SendMessage(
      params->session_id,
      params->api,
      params->input.additional_properties,
      base::Bind(&GcdPrivateSendMessageFunction::OnMessageSentCallback, this));

  return true;
}

void GcdPrivateSendMessageFunction::OnMessageSentCallback(
    api::gcd_private::Status status,
    const base::DictionaryValue& value) {
  gcd_private::PassMessage::Results::Response response;
  response.additional_properties.MergeDictionary(&value);

  results_ = gcd_private::PassMessage::Results::Create(status, response);
  SendResponse(true);
}

GcdPrivateTerminateSessionFunction::GcdPrivateTerminateSessionFunction() {
}

GcdPrivateTerminateSessionFunction::~GcdPrivateTerminateSessionFunction() {
}

bool GcdPrivateTerminateSessionFunction::RunAsync() {
  scoped_ptr<gcd_private::TerminateSession::Params> params =
      gcd_private::TerminateSession::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->RemoveSession(params->session_id);

  SendResponse(true);
  return true;
}

GcdPrivateGetCommandDefinitionsFunction::
    GcdPrivateGetCommandDefinitionsFunction() {
}

GcdPrivateGetCommandDefinitionsFunction::
    ~GcdPrivateGetCommandDefinitionsFunction() {
}

GcdPrivateGetPrefetchedWifiNameListFunction::
    GcdPrivateGetPrefetchedWifiNameListFunction() {
}

GcdPrivateGetPrefetchedWifiNameListFunction::
    ~GcdPrivateGetPrefetchedWifiNameListFunction() {
}

bool GcdPrivateGetPrefetchedWifiNameListFunction::RunSync() {
  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  scoped_ptr<base::ListValue> ssid_list = gcd_api->GetPrefetchedSSIDList();

  SetResult(ssid_list.release());

  return true;
}

bool GcdPrivateGetCommandDefinitionsFunction::RunAsync() {
  return false;
}

GcdPrivateInsertCommandFunction::GcdPrivateInsertCommandFunction() {
}

GcdPrivateInsertCommandFunction::~GcdPrivateInsertCommandFunction() {
}

bool GcdPrivateInsertCommandFunction::RunAsync() {
  return false;
}

GcdPrivateGetCommandFunction::GcdPrivateGetCommandFunction() {
}

GcdPrivateGetCommandFunction::~GcdPrivateGetCommandFunction() {
}

bool GcdPrivateGetCommandFunction::RunAsync() {
  return false;
}

GcdPrivateCancelCommandFunction::GcdPrivateCancelCommandFunction() {
}

GcdPrivateCancelCommandFunction::~GcdPrivateCancelCommandFunction() {
}

bool GcdPrivateCancelCommandFunction::RunAsync() {
  return false;
}

GcdPrivateGetCommandsListFunction::GcdPrivateGetCommandsListFunction() {
}

GcdPrivateGetCommandsListFunction::~GcdPrivateGetCommandsListFunction() {
}

bool GcdPrivateGetCommandsListFunction::RunAsync() {
  return false;
}

}  // namespace extensions
