// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/wifi_manager_nonchromeos.h"

#include "base/cancelable_callback.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "components/onc/onc_constants.h"
#include "components/wifi/wifi_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

#if defined(OS_WIN)
#include "chrome/browser/local_discovery/wifi/credential_getter_win.h"
#endif  // OS_WIN

using ::wifi::WiFiService;

namespace local_discovery {

namespace wifi {

namespace {

const int kConnectionTimeoutSeconds = 10;

scoped_ptr<base::DictionaryValue> MakeProperties(const std::string& ssid,
                                                 const std::string& password) {
  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue);

  properties->SetString(onc::network_config::kType, onc::network_type::kWiFi);
  base::DictionaryValue* wifi = new base::DictionaryValue;
  properties->Set(onc::network_config::kWiFi, wifi);

  wifi->SetString(onc::wifi::kSSID, ssid);
  if (!password.empty()) {
    wifi->SetString(onc::wifi::kPassphrase, password);
    // TODO(noamsml): Allow choosing security mechanism in a more fine-grained
    // manner.
    wifi->SetString(onc::wifi::kSecurity, onc::wifi::kWPA2_PSK);
  } else {
    wifi->SetString(onc::wifi::kSecurity, onc::wifi::kSecurityNone);
  }

  return properties.Pass();
}

}  // namespace

class WifiManagerNonChromeos::WifiServiceWrapper
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  explicit WifiServiceWrapper(
      base::WeakPtr<WifiManagerNonChromeos> wifi_manager);

  virtual ~WifiServiceWrapper();

  void Start();

  void GetSSIDList(const WifiManager::SSIDListCallback& callback);

  void ConfigureAndConnectPskNetwork(
      const std::string& ssid,
      const std::string& password,
      const WifiManager::SuccessCallback& callback);

  base::WeakPtr<WifiManagerNonChromeos::WifiServiceWrapper> AsWeakPtr();

  void RequestScan();

  void ConnectToNetworkByID(const std::string& network_guid,
                            const WifiManager::SuccessCallback& callback);

  void RequestNetworkCredentials(
      const std::string& ssid,
      const WifiManager::CredentialsCallback& callback);

 private:
  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  void GetSSIDListInternal(NetworkPropertiesList* ssid_list);

  void OnNetworkListChangedEvent(const std::vector<std::string>& network_guids);

  void OnNetworksChangedEvent(const std::vector<std::string>& network_guids);

  std::string GetConnectedGUID();

  bool IsConnected(const std::string& network_guid);

  void OnConnectToNetworkTimeout();

  void PostClosure(const base::Closure& closure);

  bool FindAndConfigureNetwork(const std::string& ssid,
                               const std::string& password,
                               std::string* network_guid);

#if defined(OS_WIN)
  void PostCredentialsCallback(const WifiManager::CredentialsCallback& callback,
                               const std::string& ssid,
                               bool success,
                               const std::string& password);
#endif  // OS_WIN

  scoped_ptr<WiFiService> wifi_service_;

  base::WeakPtr<WifiManagerNonChromeos> wifi_manager_;

  WifiManager::SuccessCallback connect_success_callback_;
  base::CancelableClosure connect_failure_callback_;

  // SSID of previously connected network.
  std::string connected_network_guid_;

  // SSID of network we are connecting to.
  std::string connecting_network_guid_;

  scoped_refptr<base::MessageLoopProxy> callback_runner_;

  base::WeakPtrFactory<WifiServiceWrapper> weak_factory_;

#if defined(OS_WIN)
  scoped_refptr<CredentialGetterWin> credential_getter_;
#endif  // OS_WIN

  DISALLOW_COPY_AND_ASSIGN(WifiServiceWrapper);
};

WifiManagerNonChromeos::WifiServiceWrapper::WifiServiceWrapper(
    base::WeakPtr<WifiManagerNonChromeos> wifi_manager)
    : wifi_manager_(wifi_manager),
      callback_runner_(base::MessageLoopProxy::current()),
      weak_factory_(this) {
}

WifiManagerNonChromeos::WifiServiceWrapper::~WifiServiceWrapper() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void WifiManagerNonChromeos::WifiServiceWrapper::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  wifi_service_.reset(WiFiService::Create());

  wifi_service_->Initialize(base::MessageLoopProxy::current());

  wifi_service_->SetEventObservers(
      base::MessageLoopProxy::current(),
      base::Bind(&WifiServiceWrapper::OnNetworksChangedEvent, AsWeakPtr()),
      base::Bind(&WifiServiceWrapper::OnNetworkListChangedEvent, AsWeakPtr()));

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

void WifiManagerNonChromeos::WifiServiceWrapper::GetSSIDList(
    const WifiManager::SSIDListCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  scoped_ptr<NetworkPropertiesList> ssid_list(new NetworkPropertiesList);
  GetSSIDListInternal(ssid_list.get());

  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiManagerNonChromeos::PostSSIDListCallback,
                 wifi_manager_,
                 callback,
                 base::Passed(&ssid_list)));
}

void WifiManagerNonChromeos::WifiServiceWrapper::ConfigureAndConnectPskNetwork(
    const std::string& ssid,
    const std::string& password,
    const WifiManager::SuccessCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  scoped_ptr<base::DictionaryValue> properties = MakeProperties(ssid, password);

  std::string network_guid;
  std::string error_string;
  // Will fail without changing system state if network already exists.
  wifi_service_->CreateNetwork(
      false, properties.Pass(), &network_guid, &error_string);

  if (error_string.empty()) {
    ConnectToNetworkByID(network_guid, callback);
    return;
  }

  // If we cannot create the network, attempt to configure and connect to an
  // existing network.
  if (FindAndConfigureNetwork(ssid, password, &network_guid)) {
    ConnectToNetworkByID(network_guid, callback);
  } else {
    if (!callback.is_null())
      PostClosure(base::Bind(callback, false /* success */));
  }
}

void WifiManagerNonChromeos::WifiServiceWrapper::OnNetworkListChangedEvent(
    const std::vector<std::string>& network_guids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  scoped_ptr<NetworkPropertiesList> ssid_list(new NetworkPropertiesList);
  GetSSIDListInternal(ssid_list.get());
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiManagerNonChromeos::OnNetworkListChanged,
                 wifi_manager_,
                 base::Passed(&ssid_list)));
}

void WifiManagerNonChromeos::WifiServiceWrapper::OnNetworksChangedEvent(
    const std::vector<std::string>& network_guids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (connecting_network_guid_.empty() ||
      !IsConnected(connecting_network_guid_)) {
    return;
  }

  connecting_network_guid_.clear();
  connect_failure_callback_.Cancel();

  if (!connect_success_callback_.is_null())
    PostClosure(base::Bind(connect_success_callback_, true));

  connect_success_callback_.Reset();
}

base::WeakPtr<WifiManagerNonChromeos::WifiServiceWrapper>
WifiManagerNonChromeos::WifiServiceWrapper::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WifiManagerNonChromeos::WifiServiceWrapper::RequestScan() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  wifi_service_->RequestNetworkScan();
}

void WifiManagerNonChromeos::WifiServiceWrapper::ConnectToNetworkByID(
    const std::string& network_guid,
    const WifiManager::SuccessCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string connected_network_id = GetConnectedGUID();
  std::string error_string;
  wifi_service_->StartConnect(network_guid, &error_string);

  if (!error_string.empty()) {
    LOG(ERROR) << "Could not connect to network by ID: " << error_string;
    PostClosure(base::Bind(callback, false /* success */));
    wifi_service_->StartConnect(connected_network_id, &error_string);
    return;
  }

  if (IsConnected(network_guid)) {
    if (!callback.is_null())
      PostClosure(base::Bind(callback, true /* success */));
    return;
  }

  connect_success_callback_ = callback;
  connecting_network_guid_ = network_guid;
  connected_network_guid_ = connected_network_id;

  connect_failure_callback_.Reset(base::Bind(
      &WifiServiceWrapper::OnConnectToNetworkTimeout, base::Unretained(this)));

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      connect_failure_callback_.callback(),
      base::TimeDelta::FromSeconds(kConnectionTimeoutSeconds));
}

void WifiManagerNonChromeos::WifiServiceWrapper::OnConnectToNetworkTimeout() {
  bool connected = IsConnected(connecting_network_guid_);
  std::string error_string;

  WifiManager::SuccessCallback connect_success_callback =
      connect_success_callback_;

  connect_success_callback_.Reset();

  connecting_network_guid_.clear();

  // If we did not connect, return to the network the user was originally
  // connected to.
  if (!connected)
    wifi_service_->StartConnect(connected_network_guid_, &error_string);

  PostClosure(base::Bind(connect_success_callback, connected /* success */));
}

void WifiManagerNonChromeos::WifiServiceWrapper::RequestNetworkCredentials(
    const std::string& ssid,
    const WifiManager::CredentialsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  bool success = true;
  std::string guid;
  std::string key;

  NetworkPropertiesList network_list;

  GetSSIDListInternal(&network_list);

  for (NetworkPropertiesList::iterator i = network_list.begin();
       i != network_list.end();
       i++) {
    if (i->ssid == ssid) {
      guid = i->guid;
      break;
    }
  }

  if (guid.empty()) {
    success = false;
  }

  if (!success) {
    PostClosure(base::Bind(callback, success, "", ""));
    return;
  }

#if defined(OS_WIN)
  credential_getter_ = new CredentialGetterWin();
  credential_getter_->StartGetCredentials(
      guid,
      base::Bind(&WifiServiceWrapper::PostCredentialsCallback,
                 AsWeakPtr(),
                 callback,
                 ssid));
#else
  if (success) {
    std::string error_string;
    wifi_service_->GetKeyFromSystem(guid, &key, &error_string);

    if (!error_string.empty()) {
      LOG(ERROR) << "Could not get key from system: " << error_string;
      success = false;
    }

    PostClosure(base::Bind(callback, success, ssid, key));
  }
#endif  // OS_WIN
}

void WifiManagerNonChromeos::WifiServiceWrapper::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  wifi_service_->RequestConnectedNetworkUpdate();
}

void WifiManagerNonChromeos::WifiServiceWrapper::GetSSIDListInternal(
    NetworkPropertiesList* ssid_list) {
  base::ListValue visible_networks;

  wifi_service_->GetVisibleNetworks(
      onc::network_type::kWiFi, &visible_networks, true);

  for (size_t i = 0; i < visible_networks.GetSize(); i++) {
    const base::DictionaryValue* network_value = NULL;
    NetworkProperties network_properties;

    if (!visible_networks.GetDictionary(i, &network_value)) {
      NOTREACHED();
    }

    network_properties.UpdateFromValue(*network_value);

    ssid_list->push_back(network_properties);
  }
}

std::string WifiManagerNonChromeos::WifiServiceWrapper::GetConnectedGUID() {
  NetworkPropertiesList ssid_list;
  GetSSIDListInternal(&ssid_list);

  for (NetworkPropertiesList::const_iterator it = ssid_list.begin();
       it != ssid_list.end();
       ++it) {
    if (it->connection_state == onc::connection_state::kConnected)
      return it->guid;
  }

  return "";
}

bool WifiManagerNonChromeos::WifiServiceWrapper::IsConnected(
    const std::string& network_guid) {
  NetworkPropertiesList ssid_list;
  GetSSIDListInternal(&ssid_list);

  for (NetworkPropertiesList::const_iterator it = ssid_list.begin();
       it != ssid_list.end();
       ++it) {
    if (it->connection_state == onc::connection_state::kConnected &&
        it->guid == network_guid)
      return true;
  }

  return false;
}

bool WifiManagerNonChromeos::WifiServiceWrapper::FindAndConfigureNetwork(
    const std::string& ssid,
    const std::string& password,
    std::string* network_guid) {
  std::string provisional_network_guid;
  NetworkPropertiesList network_property_list;
  GetSSIDListInternal(&network_property_list);

  for (size_t i = 0; i < network_property_list.size(); i++) {
    // TODO(noamsml): Handle case where there are multiple networks with the
    // same SSID but different security.
    if (network_property_list[i].ssid == ssid) {
      provisional_network_guid = network_property_list[i].guid;
      break;
    }
  }

  if (provisional_network_guid.empty())
    return false;

  scoped_ptr<base::DictionaryValue> properties = MakeProperties(ssid, password);

  std::string error_string;
  wifi_service_->SetProperties(
      provisional_network_guid, properties.Pass(), &error_string);

  if (!error_string.empty()) {
    LOG(ERROR) << "Could not set properties on network: " << error_string;
    return false;
  }

  *network_guid = provisional_network_guid;
  return true;
}

void WifiManagerNonChromeos::WifiServiceWrapper::PostClosure(
    const base::Closure& closure) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiManagerNonChromeos::PostClosure, wifi_manager_, closure));
}

#if defined(OS_WIN)
void WifiManagerNonChromeos::WifiServiceWrapper::PostCredentialsCallback(
    const WifiManager::CredentialsCallback& callback,
    const std::string& ssid,
    bool success,
    const std::string& password) {
  PostClosure(base::Bind(callback, success, ssid, password));
}

#endif  // OS_WIN

scoped_ptr<WifiManager> WifiManager::CreateDefault() {
  return scoped_ptr<WifiManager>(new WifiManagerNonChromeos());
}

WifiManagerNonChromeos::WifiManagerNonChromeos()
    : wifi_wrapper_(NULL), weak_factory_(this) {
}

WifiManagerNonChromeos::~WifiManagerNonChromeos() {
  if (wifi_wrapper_) {
    content::BrowserThread::DeleteSoon(
        content::BrowserThread::FILE, FROM_HERE, wifi_wrapper_);
  }
}

void WifiManagerNonChromeos::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::FILE);

  // Allocated on UI thread, but all initialization is done on file
  // thread. Destroyed on file thread, which should be safe since all of the
  // thread-unsafe members are created on the file thread.
  wifi_wrapper_ = new WifiServiceWrapper(weak_factory_.GetWeakPtr());

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiServiceWrapper::Start, wifi_wrapper_->AsWeakPtr()));
}

void WifiManagerNonChromeos::GetSSIDList(const SSIDListCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&WifiServiceWrapper::GetSSIDList,
                                    wifi_wrapper_->AsWeakPtr(),
                                    callback));
}

void WifiManagerNonChromeos::RequestScan() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiServiceWrapper::RequestScan, wifi_wrapper_->AsWeakPtr()));
}

void WifiManagerNonChromeos::OnNetworkListChanged(
    scoped_ptr<NetworkPropertiesList> ssid_list) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  FOR_EACH_OBSERVER(NetworkListObserver,
                    network_list_observers_,
                    OnNetworkListChanged(*ssid_list));
}

void WifiManagerNonChromeos::PostClosure(const base::Closure& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  callback.Run();
}

void WifiManagerNonChromeos::PostSSIDListCallback(
    const SSIDListCallback& callback,
    scoped_ptr<NetworkPropertiesList> ssid_list) {
  callback.Run(*ssid_list);
}

void WifiManagerNonChromeos::ConfigureAndConnectNetwork(
    const std::string& ssid,
    const WifiCredentials& credentials,
    const SuccessCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiServiceWrapper::ConfigureAndConnectPskNetwork,
                 wifi_wrapper_->AsWeakPtr(),
                 ssid,
                 credentials.psk,
                 callback));
}

void WifiManagerNonChromeos::ConnectToNetworkByID(
    const std::string& internal_id,
    const SuccessCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&WifiServiceWrapper::ConnectToNetworkByID,
                                    wifi_wrapper_->AsWeakPtr(),
                                    internal_id,
                                    callback));
}

void WifiManagerNonChromeos::RequestNetworkCredentials(
    const std::string& ssid,
    const CredentialsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WifiServiceWrapper::RequestNetworkCredentials,
                 wifi_wrapper_->AsWeakPtr(),
                 ssid,
                 callback));
}

void WifiManagerNonChromeos::AddNetworkListObserver(
    NetworkListObserver* observer) {
  network_list_observers_.AddObserver(observer);
}

void WifiManagerNonChromeos::RemoveNetworkListObserver(
    NetworkListObserver* observer) {
  network_list_observers_.RemoveObserver(observer);
}

}  // namespace wifi

}  // namespace local_discovery
