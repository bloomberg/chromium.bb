// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/timer.h"
#include "third_party/cros/chromeos_network.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

class NetworkDeviceParser;
class NetworkParser;

// This is the list of all implementation classes that are allowed
// access to the internals of the network library classes.
#define NETWORK_LIBRARY_IMPL_FRIENDS            \
  friend class NetworkLibraryImplBase;          \
  friend class NetworkLibraryImplCros;          \
  friend class NetworkLibraryImplStub;

// This enumerates the various property indices that can be found in a
// dictionary being parsed.
enum PropertyIndex {
  PROPERTY_INDEX_ACTIVATION_STATE,
  PROPERTY_INDEX_ACTIVE_PROFILE,
  PROPERTY_INDEX_ARP_GATEWAY,
  PROPERTY_INDEX_AUTO_CONNECT,
  PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES,
  PROPERTY_INDEX_CARRIER,
  PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING,
  PROPERTY_INDEX_CELLULAR_APN,
  PROPERTY_INDEX_CELLULAR_APN_LIST,
  PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN,
  PROPERTY_INDEX_CHECK_PORTAL_LIST,
  PROPERTY_INDEX_CONNECTABLE,
  PROPERTY_INDEX_CONNECTED_TECHNOLOGIES,
  PROPERTY_INDEX_CONNECTIVITY_STATE,
  PROPERTY_INDEX_DEFAULT_TECHNOLOGY,
  PROPERTY_INDEX_DEVICE,
  PROPERTY_INDEX_DEVICES,
  PROPERTY_INDEX_EAP,
  PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY,
  PROPERTY_INDEX_EAP_CA_CERT,
  PROPERTY_INDEX_EAP_CA_CERT_ID,
  PROPERTY_INDEX_EAP_CA_CERT_NSS,
  PROPERTY_INDEX_EAP_CERT_ID,
  PROPERTY_INDEX_EAP_CLIENT_CERT,
  PROPERTY_INDEX_EAP_CLIENT_CERT_NSS,
  PROPERTY_INDEX_EAP_CLIENT_CERT_PATTERN,
  PROPERTY_INDEX_EAP_IDENTITY,
  PROPERTY_INDEX_EAP_KEY_ID,
  PROPERTY_INDEX_EAP_KEY_MGMT,
  PROPERTY_INDEX_EAP_METHOD,
  PROPERTY_INDEX_EAP_PASSWORD,
  PROPERTY_INDEX_EAP_PHASE_2_AUTH,
  PROPERTY_INDEX_EAP_PIN,
  PROPERTY_INDEX_EAP_PRIVATE_KEY,
  PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD,
  PROPERTY_INDEX_EAP_USE_SYSTEM_CAS,
  PROPERTY_INDEX_ENABLED_TECHNOLOGIES,
  PROPERTY_INDEX_ERROR,
  PROPERTY_INDEX_ESN,
  PROPERTY_INDEX_FAVORITE,
  PROPERTY_INDEX_FIRMWARE_REVISION,
  PROPERTY_INDEX_FOUND_NETWORKS,
  PROPERTY_INDEX_GUID,
  PROPERTY_INDEX_HARDWARE_REVISION,
  PROPERTY_INDEX_HIDDEN_SSID,
  PROPERTY_INDEX_HOME_PROVIDER,
  PROPERTY_INDEX_HOST,
  PROPERTY_INDEX_IDENTITY,
  PROPERTY_INDEX_IMEI,
  PROPERTY_INDEX_IMSI,
  PROPERTY_INDEX_IS_ACTIVE,
  PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS,
  PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID,
  PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT,
  PROPERTY_INDEX_L2TPIPSEC_PASSWORD,
  PROPERTY_INDEX_L2TPIPSEC_PIN,
  PROPERTY_INDEX_L2TPIPSEC_PSK,
  PROPERTY_INDEX_L2TPIPSEC_USER,
  PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME,
  PROPERTY_INDEX_MANUFACTURER,
  PROPERTY_INDEX_MDN,
  PROPERTY_INDEX_MEID,
  PROPERTY_INDEX_MIN,
  PROPERTY_INDEX_MODE,
  PROPERTY_INDEX_MODEL_ID,
  PROPERTY_INDEX_NAME,
  PROPERTY_INDEX_NETWORKS,
  PROPERTY_INDEX_NETWORK_TECHNOLOGY,
  PROPERTY_INDEX_OFFLINE_MODE,
  PROPERTY_INDEX_OPERATOR_CODE,
  PROPERTY_INDEX_OPERATOR_NAME,
  PROPERTY_INDEX_PASSPHRASE,
  PROPERTY_INDEX_PASSPHRASE_REQUIRED,
  PROPERTY_INDEX_PORTAL_URL,
  PROPERTY_INDEX_POWERED,
  PROPERTY_INDEX_PRIORITY,
  PROPERTY_INDEX_PRL_VERSION,
  PROPERTY_INDEX_PROFILE,
  PROPERTY_INDEX_PROFILES,
  PROPERTY_INDEX_PROVIDER,
  PROPERTY_INDEX_PROXY_CONFIG,
  PROPERTY_INDEX_REMOVE,
  PROPERTY_INDEX_ROAMING_STATE,
  PROPERTY_INDEX_SAVE_CREDENTIALS,
  PROPERTY_INDEX_SCANNING,
  PROPERTY_INDEX_SECURITY,
  PROPERTY_INDEX_SELECTED_NETWORK,
  PROPERTY_INDEX_SERVICES,
  PROPERTY_INDEX_SERVICE_WATCH_LIST,
  PROPERTY_INDEX_SERVING_OPERATOR,
  PROPERTY_INDEX_SIGNAL_STRENGTH,
  PROPERTY_INDEX_SIM_LOCK,
  PROPERTY_INDEX_SSID,
  PROPERTY_INDEX_STATE,
  PROPERTY_INDEX_SUPPORT_NETWORK_SCAN,
  PROPERTY_INDEX_TECHNOLOGY_FAMILY,
  PROPERTY_INDEX_TYPE,
  PROPERTY_INDEX_UI_DATA,
  PROPERTY_INDEX_UNKNOWN,
  PROPERTY_INDEX_USAGE_URL,
  PROPERTY_INDEX_OLP,
  PROPERTY_INDEX_OPEN_VPN_USER,
  PROPERTY_INDEX_OPEN_VPN_PASSWORD,
  PROPERTY_INDEX_OPEN_VPN_CLIENT_CERT_ID,
  PROPERTY_INDEX_WIFI_AUTH_MODE,
  PROPERTY_INDEX_WIFI_FREQUENCY,
  PROPERTY_INDEX_WIFI_HEX_SSID,
  PROPERTY_INDEX_WIFI_HIDDEN_SSID,
  PROPERTY_INDEX_WIFI_PHY_MODE,
};

// Connection enums (see flimflam/include/service.h)
enum ConnectionType {
  TYPE_UNKNOWN   = 0,
  TYPE_ETHERNET  = 1,
  TYPE_WIFI      = 2,
  TYPE_WIMAX     = 3,
  TYPE_BLUETOOTH = 4,
  TYPE_CELLULAR  = 5,
  TYPE_VPN       = 6,
};

enum ConnectionMode {
  MODE_UNKNOWN = 0,
  MODE_MANAGED = 1,
  MODE_ADHOC   = 2,
};

enum ConnectionSecurity {
  SECURITY_UNKNOWN = 0,
  SECURITY_NONE    = 1,
  SECURITY_WEP     = 2,
  SECURITY_WPA     = 3,
  SECURITY_RSN     = 4,
  SECURITY_8021X   = 5,
  SECURITY_PSK     = 6,
};

enum ConnectionState {
  STATE_UNKNOWN            = 0,
  STATE_IDLE               = 1,
  STATE_CARRIER            = 2,
  STATE_ASSOCIATION        = 3,
  STATE_CONFIGURATION      = 4,
  STATE_READY              = 5,
  STATE_DISCONNECT         = 6,
  STATE_FAILURE            = 7,
  STATE_ACTIVATION_FAILURE = 8,
  STATE_PORTAL             = 9,
  STATE_ONLINE             = 10
};

// Network enums (see flimflam/include/network.h)
enum NetworkTechnology {
  NETWORK_TECHNOLOGY_UNKNOWN      = 0,
  NETWORK_TECHNOLOGY_1XRTT        = 1,
  NETWORK_TECHNOLOGY_EVDO         = 2,
  NETWORK_TECHNOLOGY_GPRS         = 3,
  NETWORK_TECHNOLOGY_EDGE         = 4,
  NETWORK_TECHNOLOGY_UMTS         = 5,
  NETWORK_TECHNOLOGY_HSPA         = 6,
  NETWORK_TECHNOLOGY_HSPA_PLUS    = 7,
  NETWORK_TECHNOLOGY_LTE          = 8,
  NETWORK_TECHNOLOGY_LTE_ADVANCED = 9,
  NETWORK_TECHNOLOGY_GSM          = 10,
};

enum ActivationState {
  ACTIVATION_STATE_UNKNOWN             = 0,
  ACTIVATION_STATE_ACTIVATED           = 1,
  ACTIVATION_STATE_ACTIVATING          = 2,
  ACTIVATION_STATE_NOT_ACTIVATED       = 3,
  ACTIVATION_STATE_PARTIALLY_ACTIVATED = 4,
};

enum NetworkRoamingState {
  ROAMING_STATE_UNKNOWN = 0,
  ROAMING_STATE_HOME    = 1,
  ROAMING_STATE_ROAMING = 2,
};

// Device enums (see flimflam/include/device.h)
enum TechnologyFamily {
  TECHNOLOGY_FAMILY_UNKNOWN = 0,
  TECHNOLOGY_FAMILY_CDMA    = 1,
  TECHNOLOGY_FAMILY_GSM     = 2
};

// Type of a pending SIM operation.
enum SimOperationType {
  SIM_OPERATION_NONE               = 0,
  SIM_OPERATION_CHANGE_PIN         = 1,
  SIM_OPERATION_CHANGE_REQUIRE_PIN = 2,
  SIM_OPERATION_ENTER_PIN          = 3,
  SIM_OPERATION_UNBLOCK_PIN        = 4,
};

// SIMLock states (see gobi-cromo-plugin/gobi_gsm_modem.cc)
enum SimLockState {
  SIM_UNKNOWN    = 0,
  SIM_UNLOCKED   = 1,
  SIM_LOCKED_PIN = 2,
  SIM_LOCKED_PUK = 3,  // also when SIM is blocked, then retries = 0.
};

// SIM PinRequire states.
// SIM_PIN_REQUIRE_UNKNOWN - SIM card is absent or SimLockState initial value
//                           hasn't been received yet.
// SIM_PIN_REQUIRED - SIM card is locked when booted/wake from sleep and
//                    requires user to enter PIN.
// SIM_PIN_NOT_REQUIRED - SIM card is unlocked all the time and requires PIN
// only on certain operations, such as ChangeRequirePin, ChangePin, EnterPin.
enum SimPinRequire {
  SIM_PIN_REQUIRE_UNKNOWN = 0,
  SIM_PIN_NOT_REQUIRED    = 1,
  SIM_PIN_REQUIRED        = 2,
};

// Any PIN operation result (EnterPin, UnblockPin etc.).
enum PinOperationError {
  PIN_ERROR_NONE           = 0,
  PIN_ERROR_UNKNOWN        = 1,
  PIN_ERROR_INCORRECT_CODE = 2,  // Either PIN/PUK specified is incorrect.
  PIN_ERROR_BLOCKED        = 3,  // No more PIN retries left, SIM is blocked.
};

// connection errors (see flimflam/include/service.h)
enum ConnectionError {
  ERROR_NO_ERROR               = 0,
  ERROR_OUT_OF_RANGE           = 1,
  ERROR_PIN_MISSING            = 2,
  ERROR_DHCP_FAILED            = 3,
  ERROR_CONNECT_FAILED         = 4,
  ERROR_BAD_PASSPHRASE         = 5,
  ERROR_BAD_WEPKEY             = 6,
  ERROR_ACTIVATION_FAILED      = 7,
  ERROR_NEED_EVDO              = 8,
  ERROR_NEED_HOME_NETWORK      = 9,
  ERROR_OTASP_FAILED           = 10,
  ERROR_AAA_FAILED             = 11,
  ERROR_INTERNAL               = 12,
  ERROR_DNS_LOOKUP_FAILED      = 13,
  ERROR_HTTP_GET_FAILED        = 14,
  ERROR_IPSEC_PSK_AUTH_FAILED  = 15,
  ERROR_IPSEC_CERT_AUTH_FAILED = 16,
  ERROR_PPP_AUTH_FAILED        = 17,
  ERROR_UNKNOWN                = 255
};

// We are currently only supporting setting a single EAP Method.
enum EAPMethod {
  EAP_METHOD_UNKNOWN = 0,
  EAP_METHOD_PEAP    = 1,
  EAP_METHOD_TLS     = 2,
  EAP_METHOD_TTLS    = 3,
  EAP_METHOD_LEAP    = 4
};

// We are currently only supporting setting a single EAP phase 2 authentication.
enum EAPPhase2Auth {
  EAP_PHASE_2_AUTH_AUTO     = 0,
  EAP_PHASE_2_AUTH_MD5      = 1,
  EAP_PHASE_2_AUTH_MSCHAPV2 = 2,
  EAP_PHASE_2_AUTH_MSCHAP   = 3,
  EAP_PHASE_2_AUTH_PAP      = 4,
  EAP_PHASE_2_AUTH_CHAP     = 5
};

// Misc enums
enum NetworkProfileType {
  PROFILE_NONE,    // Not in any profile (not remembered).
  PROFILE_SHARED,  // In the local profile, shared by all users on device.
  PROFILE_USER     // In the user provile, not visible to other users.
};

// Virtual Network provider type.
enum ProviderType {
  PROVIDER_TYPE_L2TP_IPSEC_PSK,
  PROVIDER_TYPE_L2TP_IPSEC_USER_CERT,
  PROVIDER_TYPE_OPEN_VPN,
  // Add new provider types before PROVIDER_TYPE_MAX.
  PROVIDER_TYPE_MAX,
};

// Simple wrapper for property Cellular.FoundNetworks.
struct FoundCellularNetwork {
  FoundCellularNetwork();
  ~FoundCellularNetwork();

  std::string status;
  std::string network_id;
  std::string short_name;
  std::string long_name;
  std::string technology;
};
typedef std::vector<FoundCellularNetwork> CellularNetworkList;

struct CellularApn {
  std::string apn;
  std::string network_id;
  std::string username;
  std::string password;
  std::string name;
  std::string localized_name;
  std::string language;

  CellularApn();
  CellularApn(const std::string& apn, const std::string& network_id,
      const std::string& username, const std::string& password);
  ~CellularApn();
  void Set(const base::DictionaryValue& dict);
};
typedef std::vector<CellularApn> CellularApnList;

// Cellular network is considered low data when less than 60 minues.
static const int kCellularDataLowSecs = 60 * 60;

// Cellular network is considered low data when less than 30 minues.
static const int kCellularDataVeryLowSecs = 30 * 60;

// Cellular network is considered low data when less than 100MB.
static const int kCellularDataLowBytes = 100 * 1024 * 1024;

// Cellular network is considered very low data when less than 50MB.
static const int kCellularDataVeryLowBytes = 50 * 1024 * 1024;

// The value of priority if it is not set.
const int kPriorityNotSet = 0;
// The value of priority if network is preferred.
const int kPriorityPreferred = 1;

// Contains data related to the flimflam.Device interface,
// e.g. ethernet, wifi, cellular.
// TODO(dpolukhin): refactor to make base class and device specific derivatives.
class NetworkDevice {
 public:
  explicit NetworkDevice(const std::string& device_path);
  ~NetworkDevice();

  NetworkDeviceParser* device_parser() { return device_parser_.get(); }
  void SetNetworkDeviceParser(NetworkDeviceParser* parser);

  // Device info.
  const std::string& device_path() const { return device_path_; }
  const std::string& name() const { return name_; }
  const std::string& unique_id() const { return unique_id_; }
  ConnectionType type() const { return type_; }
  bool scanning() const { return scanning_; }
  const std::string& meid() const { return meid_; }
  const std::string& imei() const { return imei_; }
  const std::string& imsi() const { return imsi_; }
  const std::string& esn() const { return esn_; }
  const std::string& mdn() const { return mdn_; }
  const std::string& min() const { return min_; }
  const std::string& model_id() const { return model_id_; }
  const std::string& manufacturer() const { return manufacturer_; }
  SimLockState sim_lock_state() const { return sim_lock_state_; }
  bool is_sim_locked() const {
    return sim_lock_state_ == SIM_LOCKED_PIN ||
        sim_lock_state_ == SIM_LOCKED_PUK;
  }
  const int sim_retries_left() const { return sim_retries_left_; }
  SimPinRequire sim_pin_required() const { return sim_pin_required_; }
  const std::string& firmware_revision() const { return firmware_revision_; }
  const std::string& hardware_revision() const { return hardware_revision_; }
  const unsigned int prl_version() const { return prl_version_; }
  const std::string& home_provider_code() const { return home_provider_code_; }
  const std::string& home_provider_country() const {
    return home_provider_country_;
  }
  const std::string& home_provider_id() const { return home_provider_id_; }
  const std::string& home_provider_name() const { return home_provider_name_; }
  const std::string& selected_cellular_network() const {
    return selected_cellular_network_;
  }
  const CellularNetworkList& found_cellular_networks() const {
    return found_cellular_networks_;
  }
  bool data_roaming_allowed() const { return data_roaming_allowed_; }
  bool support_network_scan() const { return support_network_scan_; }
  enum TechnologyFamily technology_family() const { return technology_family_; }
  const CellularApnList& provider_apn_list() const {
    return provider_apn_list_;
  }

  // Updates the property specified by |key| with the contents of
  // |value|.  Returns false on failure.  Upon success, returns the
  // PropertyIndex that was updated in |index|.  |index| may be NULL
  // if not needed.
  bool UpdateStatus(const std::string& key,
                    const base::Value& value,
                    PropertyIndex* index);

 protected:
  void set_unique_id(const std::string& unique_id) { unique_id_ = unique_id; }

 private:
  // This allows NetworkDeviceParser and its subclasses access to
  // device privates so that they can be reconstituted during parsing.
  // The parsers only access things through the private set_ functions
  // so that this class can evolve without having to change all the
  // parsers.
  friend class NativeNetworkDeviceParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // Use these functions at your peril.  They are used by the various
  // parsers to set state, and really shouldn't be used by anyone
  // else.
  void set_device_path(const std::string& device_path) {
    device_path_ = device_path;
  }
  void set_name(const std::string& name) { name_ = name; }
  void set_type(ConnectionType type) { type_ = type; }
  void set_scanning(bool scanning) { scanning_ = scanning; }
  void set_meid(const std::string& meid) { meid_ = meid; }
  void set_imei(const std::string& imei) { imei_ = imei; }
  void set_imsi(const std::string& imsi) { imsi_ = imsi; }
  void set_esn(const std::string& esn) { esn_ = esn; }
  void set_mdn(const std::string& mdn) { mdn_ = mdn; }
  void set_min(const std::string& min) { min_ = min; }
  void set_technology_family(TechnologyFamily technology_family) {
    technology_family_ = technology_family;
  }
  void set_carrier(const std::string& carrier) { carrier_ = carrier; }
  void set_home_provider_code(const std::string& home_provider_code) {
    home_provider_code_ = home_provider_code;
  }
  void set_home_provider_country(const std::string& home_provider_country) {
    home_provider_country_ = home_provider_country;
  }
  void set_home_provider_id(const std::string& home_provider_id) {
    home_provider_id_ = home_provider_id;
  }
  void set_home_provider_name(const std::string& home_provider_name) {
    home_provider_name_ = home_provider_name;
  }
  void set_model_id(const std::string& model_id) { model_id_ = model_id; }
  void set_manufacturer(const std::string& manufacturer) {
    manufacturer_ = manufacturer;
  }
  void set_prl_version(int prl_version) {
    prl_version_ = prl_version;
  }
  void set_sim_lock_state(SimLockState sim_lock_state) {
    sim_lock_state_ = sim_lock_state;
  }
  void set_sim_retries_left(int sim_retries_left) {
    sim_retries_left_ = sim_retries_left;
  }
  void set_sim_pin_required(SimPinRequire sim_pin_required) {
    sim_pin_required_ = sim_pin_required;
  }
  void set_firmware_revision(const std::string& firmware_revision) {
    firmware_revision_ = firmware_revision;
  }
  void set_hardware_revision(const std::string& hardware_revision) {
    hardware_revision_ = hardware_revision;
  }
  void set_selected_cellular_network(
      const std::string& selected_cellular_network) {
    selected_cellular_network_ = selected_cellular_network;
  }
  void set_found_cellular_networks(
      const CellularNetworkList& found_cellular_networks) {
    found_cellular_networks_ = found_cellular_networks;
  }
  void set_data_roaming_allowed(bool data_roaming_allowed) {
    data_roaming_allowed_ = data_roaming_allowed;
  }
  void set_support_network_scan(bool support_network_scan) {
    support_network_scan_ = support_network_scan;
  }
  void set_provider_apn_list(const CellularApnList& provider_apn_list) {
    provider_apn_list_ = provider_apn_list;
  }

  void ParseInfo(const base::DictionaryValue& info);

  // General device info.
  std::string device_path_;
  std::string name_;
  std::string unique_id_;
  ConnectionType type_;
  bool scanning_;
  // Cellular specific device info.
  TechnologyFamily technology_family_;
  std::string carrier_;
  std::string home_provider_code_;
  std::string home_provider_country_;
  std::string home_provider_id_;
  std::string home_provider_name_;
  std::string meid_;
  std::string imei_;
  std::string imsi_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  SimLockState sim_lock_state_;
  int sim_retries_left_;
  SimPinRequire sim_pin_required_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  int prl_version_;
  std::string selected_cellular_network_;
  CellularNetworkList found_cellular_networks_;
  bool data_roaming_allowed_;
  bool support_network_scan_;
  CellularApnList provider_apn_list_;

  // This is the parser we use to parse messages from the native
  // network layer.
  scoped_ptr<NetworkDeviceParser> device_parser_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDevice);
};

// Contains data common to all network service types.
class Network {
 public:
  virtual ~Network();

  // Test API for accessing setters in tests.
  class TestApi {
   public:
    explicit TestApi(Network* network) : network_(network) {}
    void SetConnected(bool connected) {
      network_->set_connected(connected);
    }
    void SetConnecting(bool connecting) {
      network_->set_connecting(connecting);
    }
   private:
    Network* network_;
  };
  friend class TestApi;

  const std::string& service_path() const { return service_path_; }
  const std::string& name() const { return name_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& ip_address() const { return ip_address_; }
  ConnectionType type() const { return type_; }
  ConnectionMode mode() const { return mode_; }
  ConnectionState connection_state() const { return state_; }
  bool connecting() const { return IsConnectingState(state_); }
  bool configuring() const { return state_ == STATE_CONFIGURATION; }
  bool connected() const { return IsConnectedState(state_); }
  bool connecting_or_connected() const { return connecting() || connected(); }
  // True when a user-initiated connection attempt is in progress
  bool connection_started() const { return connection_started_; }
  bool failed() const { return state_ == STATE_FAILURE; }
  bool disconnected() const { return IsDisconnectedState(state_); }
  bool ready() const { return state_ == STATE_READY; }
  bool online() const { return state_ == STATE_ONLINE; }
  bool restricted_pool() const { return state_ == STATE_PORTAL; }
  ConnectionError error() const { return error_; }
  ConnectionState state() const { return state_; }
  // Is this network connectable. Currently, this is mainly used by 802.1x
  // networks to specify that the network is not configured yet.
  bool connectable() const { return connectable_; }
  // Is this the active network, i.e, the one through which
  // network traffic is being routed? A network can be connected,
  // but not be carrying traffic.
  bool is_active() const { return is_active_; }
  bool preferred() const { return priority_ != kPriorityNotSet; }
  bool auto_connect() const { return auto_connect_; }
  bool save_credentials() const { return save_credentials_; }

  bool added() const { return added_; }
  bool notify_failure() const { return notify_failure_; }
  const std::string& profile_path() const { return profile_path_; }
  NetworkProfileType profile_type() const { return profile_type_; }

  const std::string& unique_id() const { return unique_id_; }
  int priority_order() const { return priority_order_; }

  const std::string& proxy_config() const { return proxy_config_; }

  void set_notify_failure(bool state) { notify_failure_ = state; }

  void SetPreferred(bool preferred);

  void SetAutoConnect(bool auto_connect);

  void SetName(const std::string& name);

  void SetSaveCredentials(bool save_credentials);

  // Return a string representation of the state code.
  std::string GetStateString() const;

  // Return a string representation of the error code.
  std::string GetErrorString() const;

  void SetProxyConfig(const std::string& proxy_config);

  // Return true if the network must be in the user profile (e.g. has certs).
  virtual bool RequiresUserProfile() const;

  // Copy any credentials from a remembered network that are unset in |this|.
  virtual void CopyCredentialsFromRemembered(Network* remembered);

  // Static helper functions.
  static bool IsConnectedState(ConnectionState state) {
    return (state == STATE_READY ||
            state == STATE_ONLINE ||
            state == STATE_PORTAL);
  }
  static bool IsConnectingState(ConnectionState state) {
    return (state == STATE_ASSOCIATION ||
            state == STATE_CONFIGURATION ||
            state == STATE_CARRIER);
  }
  static bool IsDisconnectedState(ConnectionState state) {
    return (state == STATE_UNKNOWN ||
            state == STATE_IDLE ||
            state == STATE_DISCONNECT ||
            state == STATE_FAILURE ||
            state == STATE_ACTIVATION_FAILURE);
  }

  virtual bool UpdateStatus(const std::string& key,
                            const base::Value& value,
                            PropertyIndex* index);

 protected:
  Network(const std::string& service_path,
          ConnectionType type);

  NetworkParser* network_parser() { return network_parser_.get(); }
  void SetNetworkParser(NetworkParser* parser);

  // Updates property_map_ for the corresponding property index.
  void UpdatePropertyMap(PropertyIndex index, const base::Value& value);

  // Set the state and update flags if necessary.
  void SetState(ConnectionState state);

  // Parse name/value pairs from libcros.
  virtual void ParseInfo(const base::DictionaryValue& info);

  // Erase cached credentials, used when "Save password" is unchecked.
  virtual void EraseCredentials();

  // Calculate a unique identifier for the network.
  virtual void CalculateUniqueId();

  // Methods to asynchronously set network service properties
  virtual void SetStringProperty(const char* prop, const std::string& str,
                                 std::string* dest);
  virtual void SetBooleanProperty(const char* prop, bool b, bool* dest);
  virtual void SetIntegerProperty(const char* prop, int i, int* dest);
  virtual void SetValueProperty(const char* prop, base::Value* val);
  virtual void ClearProperty(const char* prop);

  // This will clear the property if string is empty. Otherwise, it will set it.
  virtual void SetOrClearStringProperty(const char* prop,
                                        const std::string& str,
                                        std::string* dest);

  void set_unique_id(const std::string& unique_id) { unique_id_ = unique_id; }
  DictionaryValue* ui_data() { return &ui_data_; }

 private:
  typedef std::map<PropertyIndex, base::Value*> PropertyMap;

  // This allows NetworkParser and its subclasses access to device
  // privates so that they can be reconstituted during parsing.  The
  // parsers only access things through the private set_ functions so
  // that this class can evolve without having to change all the
  // parsers.
  friend class NetworkParser;
  friend class NativeNetworkParser;
  friend class NativeVirtualNetworkParser;
  friend class OncNetworkParser;
  friend class OncVirtualNetworkParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // Use these functions at your peril.  They are used by the various
  // parsers to set state, and really shouldn't be used by anything else
  // because they don't do the error checking and sending to the
  // network layer that the other setters do.
  void set_device_path(const std::string& device_path) {
    device_path_ = device_path;
  }
  void set_name(const std::string& name) { name_ = name; }
  void set_mode(ConnectionMode mode) { mode_ = mode; }
  void set_connecting(bool connecting) {
    state_ = (connecting ? STATE_ASSOCIATION : STATE_IDLE);
  }
  void set_connected(bool connected) {
    state_ = (connected ? STATE_ONLINE : STATE_IDLE);
  }
  void set_connectable(bool connectable) { connectable_ = connectable; }
  void set_connection_started(bool started) { connection_started_ = started; }
  void set_is_active(bool is_active) { is_active_ = is_active; }
  void set_error(ConnectionError error) { error_ = error; }
  void set_added(bool added) { added_ = added; }
  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }
  void set_save_credentials(bool save_credentials) {
    save_credentials_ = save_credentials;
  }
  void set_profile_path(const std::string& path) { profile_path_ = path; }
  void set_profile_type(NetworkProfileType type) { profile_type_ = type; }
  void set_proxy_config(const std::string& proxy) { proxy_config_ = proxy; }

  std::string device_path_;
  std::string name_;
  std::string ip_address_;
  ConnectionMode mode_;
  ConnectionState state_;
  ConnectionError error_;
  bool connectable_;
  bool connection_started_;
  bool is_active_;
  int priority_;  // determines order in network list.
  bool auto_connect_;
  bool save_credentials_;  // save passphrase and EAP credentials to disk.
  std::string proxy_config_;

  // Unique identifier, set the first time the network is parsed.
  std::string unique_id_;

  // Set the profile path and update the flimfalm property.
  void SetProfilePath(const std::string& profile_path);

  // Initialize the IP address field
  void InitIPAddress();

  // Priority value, corresponds to index in list from flimflam (0 = first)
  int priority_order_;

  // Set to true if the UI requested this as a new network.
  bool added_;

  // Set to true when a new connection failure occurs; cleared when observers
  // are notified.
  bool notify_failure_;

  // Profile path for networks.
  std::string profile_path_;

  // Set to profile type based on profile_path_.
  NetworkProfileType profile_type_;

  // These must not be modified after construction.
  std::string service_path_;
  ConnectionType type_;

  // UI-level state that is opaque to the connection manager. It's kept in a
  // DictionaryValue to allow for simple addition of new settings. The value is
  // stored in JSON-serialized from in the connection manager.
  DictionaryValue ui_data_;

  // This is the parser we use to parse messages from the native
  // network layer.
  scoped_ptr<NetworkParser> network_parser_;

  // This map stores the set of properties for the network.
  // Not all properties in this map are exposed via get methods.
  PropertyMap property_map_;

  DISALLOW_COPY_AND_ASSIGN(Network);
};

// Class for networks of TYPE_ETHERNET.
class EthernetNetwork : public Network {
 public:
  explicit EthernetNetwork(const std::string& service_path);
 private:
  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  DISALLOW_COPY_AND_ASSIGN(EthernetNetwork);
};

// Class for networks of TYPE_VPN.
class VirtualNetwork : public Network {
 public:
  explicit VirtualNetwork(const std::string& service_path);
  virtual ~VirtualNetwork();

  const std::string& server_hostname() const { return server_hostname_; }
  ProviderType provider_type() const { return provider_type_; }
  const std::string& ca_cert_nss() const { return ca_cert_nss_; }
  const std::string& psk_passphrase() const { return psk_passphrase_; }
  const std::string& client_cert_id() const { return client_cert_id_; }
  const std::string& username() const { return username_; }
  const std::string& user_passphrase() const { return user_passphrase_; }
  const std::string& group_name() const { return group_name_; }

  // Sets the well-known PKCS#11 slot and PIN for accessing certificates.
  void SetCertificateSlotAndPin(
      const std::string& slot, const std::string& pin);

  // Network overrides.
  virtual bool RequiresUserProfile() const OVERRIDE;
  virtual void CopyCredentialsFromRemembered(Network* remembered) OVERRIDE;

  // Public getters.
  bool NeedMoreInfoToConnect() const;
  std::string GetProviderTypeString() const;

  // Public setters.
  void SetCACertNSS(const std::string& ca_cert_nss);
  void SetL2TPIPsecPSKCredentials(const std::string& psk_passphrase,
                                  const std::string& username,
                                  const std::string& user_passphrase,
                                  const std::string& group_name);
  void SetL2TPIPsecCertCredentials(const std::string& client_cert_id,
                                   const std::string& username,
                                   const std::string& user_passphrase,
                                   const std::string& group_name);
  void SetOpenVPNCredentials(const std::string& client_cert_id,
                             const std::string& username,
                             const std::string& user_passphrase,
                             const std::string& otp);

 private:
  // This allows NetworkParser and its subclasses access to
  // device privates so that they can be reconstituted during parsing.
  // The parsers only access things through the private set_ functions
  // so that this class can evolve without having to change all the
  // parsers.
  friend class NativeNetworkParser;
  friend class NativeVirtualNetworkParser;
  friend class OncNetworkParser;
  friend class OncVirtualNetworkParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // Use these functions at your peril.  They are used by the various
  // parsers to set state, and really shouldn't be used by anything else
  // because they don't do the error checking and sending to the
  // network layer that the other setters do.
  void set_server_hostname(const std::string& server_hostname) {
    server_hostname_ = server_hostname;
  }
  void set_provider_type(ProviderType provider_type) {
    provider_type_ = provider_type;
  }
  void set_ca_cert_nss(const std::string& ca_cert_nss) {
    ca_cert_nss_ = ca_cert_nss;
  }
  void set_psk_passphrase(const std::string& psk_passphrase) {
    psk_passphrase_ = psk_passphrase;
  }
  void set_client_cert_id(const std::string& client_cert_id) {
    client_cert_id_ = client_cert_id;
  }
  void set_username(const std::string& username) { username_ = username; }
  void set_user_passphrase(const std::string& user_passphrase) {
    user_passphrase_ = user_passphrase;
  }
  void set_group_name(const std::string& group_name) {
    group_name_ = group_name;
  }

  // Network overrides.
  virtual void EraseCredentials() OVERRIDE;
  virtual void CalculateUniqueId() OVERRIDE;

  // VirtualNetwork private methods.
  bool ParseProviderValue(int index, const base::Value* value);

  std::string server_hostname_;
  ProviderType provider_type_;
  // NSS nickname for server CA certificate.
  std::string ca_cert_nss_;
  std::string psk_passphrase_;
  // PKCS#11 ID for client certificate.
  std::string client_cert_id_;
  std::string username_;
  std::string user_passphrase_;
  std::string group_name_;
  DISALLOW_COPY_AND_ASSIGN(VirtualNetwork);
};
typedef std::vector<VirtualNetwork*> VirtualNetworkVector;

// Base class for networks of TYPE_WIFI or TYPE_CELLULAR.
class WirelessNetwork : public Network {
 public:
  // Test API for accessing setters in tests.
  class TestApi {
   public:
    explicit TestApi(WirelessNetwork* network) : network_(network) {}
    void SetStrength(int strength) { network_->set_strength(strength); }
   private:
    WirelessNetwork* network_;
  };
  friend class TestApi;

  int strength() const { return strength_; }

 protected:
  WirelessNetwork(const std::string& service_path,
                  ConnectionType type)
      : Network(service_path, type), strength_(0) {}

 private:
  // This allows NativeWirelessNetworkParser access to device privates
  // so that they can be reconstituted during parsing.  The parsers
  // only access things through the private set_ functions so that
  // this class can evolve without having to change all the parsers.
  friend class NativeWirelessNetworkParser;
  friend class OncWirelessNetworkParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // The friend parsers use this.
  void set_strength(int strength) { strength_ = strength; }

  int strength_;  // 0-100

  DISALLOW_COPY_AND_ASSIGN(WirelessNetwork);
};

// Class for networks of TYPE_CELLULAR.
class CellularDataPlan;

class CellularNetwork : public WirelessNetwork {
 public:
  enum DataLeft {
    DATA_UNKNOWN,
    DATA_NORMAL,
    DATA_LOW,
    DATA_VERY_LOW,
    DATA_NONE
  };

  // Test API for accessing setters in tests.
  class TestApi {
   public:
    explicit TestApi(CellularNetwork* network) : network_(network) {}
    void SetRoamingState(NetworkRoamingState roaming_state) {
      network_->set_roaming_state(roaming_state);
    }
   private:
    CellularNetwork* network_;
  };
  friend class TestApi;

  explicit CellularNetwork(const std::string& service_path);
  virtual ~CellularNetwork();

  // Starts device activation process. Returns false if the device state does
  // not permit activation.
  bool StartActivation();
  // Requests data plans if the network is conencted and activated.
  // Plan data will be passed through Network::Observer::CellularDataPlanChanged
  // callback.
  void RefreshDataPlansIfNeeded() const;

  const ActivationState activation_state() const { return activation_state_; }
  bool activated() const {
    return activation_state() == ACTIVATION_STATE_ACTIVATED;
  }
  const NetworkTechnology network_technology() const {
    return network_technology_;
  }
  const NetworkRoamingState roaming_state() const { return roaming_state_; }
  bool needs_new_plan() const {
    return SupportsDataPlan() && restricted_pool()
        && connected() && activated();
  }
  const std::string& operator_name() const { return operator_name_; }
  const std::string& operator_code() const { return operator_code_; }
  const std::string& operator_country() const { return operator_country_; }
  const std::string& payment_url() const { return payment_url_; }
  const std::string& usage_url() const { return usage_url_; }
  const std::string& post_data() const { return post_data_; }
  const bool using_post() const { return using_post_; }
  DataLeft data_left() const { return data_left_; }
  const CellularApn& apn() const { return apn_; }
  const CellularApn& last_good_apn() const { return last_good_apn_; }

  // Sets the APN to use in establishing data connections. Only
  // the fields of the APN that are needed for making connections
  // are passed to flimflam. The name, localized_name, and language
  // fields are ignored.
  void SetApn(const CellularApn& apn);

  // Returns true if network supports activation.
  // Current implementation returns same as SupportsDataPlan().
  bool SupportsActivation() const;

  // Returns true if one of the usage_url_ / payment_url_ (or both) is defined.
  bool SupportsDataPlan() const;

  // Return a string representation of network technology.
  std::string GetNetworkTechnologyString() const;
  // Return a string representation of activation state.
  std::string GetActivationStateString() const;
  // Return a string representation of roaming state.
  std::string GetRoamingStateString() const;

  // Return a string representation of |activation_state|.
  static std::string ActivationStateToString(ActivationState activation_state);

 private:
  // This allows NativeCellularNetworkParser access to device privates
  // so that they can be reconstituted during parsing.  The parsers
  // only access things through the private set_ functions so that
  // this class can evolve without having to change all the parsers.
  friend class NativeCellularNetworkParser;
  friend class OncCellularNetworkParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // Use these functions at your peril.  They are used by the various
  // parsers to set state, and really shouldn't be used by anything else
  // because they don't do the error checking and sending to the
  // network layer that the other setters do.
  void set_activation_state(ActivationState activation_state) {
    activation_state_ = activation_state;
  }
  void set_network_technology(NetworkTechnology network_technology) {
    network_technology_ = network_technology;
  }
  void set_roaming_state(NetworkRoamingState roaming_state) {
    roaming_state_ = roaming_state;
  }
  void set_operator_name(const std::string& operator_name) {
    operator_name_ = operator_name;
  }
  void set_operator_code(const std::string& operator_code) {
    operator_code_ = operator_code;
  }
  void set_operator_country(const std::string& operator_country) {
    operator_country_ = operator_country;
  }
  void set_payment_url(const std::string& payment_url) {
    payment_url_ = payment_url;
  }
  void set_post_data(const std::string& post_data) {
    post_data_ = post_data;
  }
  void set_using_post(bool using_post) {
    using_post_ = using_post;
  }
  void set_usage_url(const std::string& usage_url) { usage_url_ = usage_url; }
  void set_data_left(DataLeft data_left) { data_left_ = data_left; }
  void set_apn(const base::DictionaryValue& apn) { apn_.Set(apn); }
  void set_last_good_apn(const base::DictionaryValue& last_good_apn) {
    last_good_apn_.Set(last_good_apn);
  }

  ActivationState activation_state_;
  NetworkTechnology network_technology_;
  NetworkRoamingState roaming_state_;
  // Carrier Info
  std::string operator_name_;
  std::string operator_code_;
  std::string operator_country_;
  std::string payment_url_;
  std::string usage_url_;
  std::string post_data_;
  bool using_post_;
  // Cached values
  DataLeft data_left_;  // Updated when data plans are updated.
  CellularApn apn_;
  CellularApn last_good_apn_;

  DISALLOW_COPY_AND_ASSIGN(CellularNetwork);
};
typedef std::vector<CellularNetwork*> CellularNetworkVector;

// Class for networks of TYPE_WIFI.
class WifiNetwork : public WirelessNetwork {
 public:
  // Test API for accessing setters in tests.
  class TestApi {
   public:
    explicit TestApi(WifiNetwork* network) : network_(network) {}
    void SetEncryption(ConnectionSecurity encryption) {
      network_->set_encryption(encryption);
    }
   private:
    WifiNetwork* network_;
  };
  friend class TestApi;

  explicit WifiNetwork(const std::string& service_path);
  virtual ~WifiNetwork();

  bool encrypted() const { return encryption_ != SECURITY_NONE; }
  ConnectionSecurity encryption() const { return encryption_; }
  const std::string& passphrase() const { return passphrase_; }
  const std::string& identity() const { return identity_; }
  bool passphrase_required() const { return passphrase_required_; }

  EAPMethod eap_method() const { return eap_method_; }
  EAPPhase2Auth eap_phase_2_auth() const { return eap_phase_2_auth_; }
  const std::string& eap_server_ca_cert_nss_nickname() const {
    return eap_server_ca_cert_nss_nickname_; }
  const std::string& eap_client_cert_pkcs11_id() const {
    return eap_client_cert_pkcs11_id_; }
  const bool eap_use_system_cas() const { return eap_use_system_cas_; }
  const std::string& eap_identity() const { return eap_identity_; }
  const std::string& eap_anonymous_identity() const {
    return eap_anonymous_identity_; }
  const std::string& eap_passphrase() const { return eap_passphrase_; }

  const std::string& GetPassphrase() const;

  bool SetSsid(const std::string& ssid);
  bool SetHexSsid(const std::string& ssid_hex);
  void SetPassphrase(const std::string& passphrase);
  void SetIdentity(const std::string& identity);

  // 802.1x properties
  void SetEAPMethod(EAPMethod method);
  void SetEAPPhase2Auth(EAPPhase2Auth auth);
  void SetEAPServerCaCertNssNickname(const std::string& nss_nickname);
  void SetEAPClientCertPkcs11Id(const std::string& pkcs11_id);
  void SetEAPUseSystemCAs(bool use_system_cas);
  void SetEAPIdentity(const std::string& identity);
  void SetEAPAnonymousIdentity(const std::string& identity);
  void SetEAPPassphrase(const std::string& passphrase);

  // Sets the well-known PKCS#11 PIN for accessing certificates.
  void SetCertificatePin(const std::string& pin);

  // Network overrides.
  virtual bool RequiresUserProfile() const OVERRIDE;

  // Return a string representation of the encryption code.
  // This not translated and should be only used for debugging purposes.
  std::string GetEncryptionString() const;

  // Return true if a passphrase or other input is required to connect.
  bool IsPassphraseRequired() const;

 private:
  // This allows NativeWifiNetworkParser access to device privates so
  // that they can be reconstituted during parsing.  The parsers only
  // access things through the private set_ functions so that this
  // class can evolve without having to change all the parsers.
  friend class NativeWifiNetworkParser;
  friend class OncWifiNetworkParser;

  // This allows the implementation classes access to privates.
  NETWORK_LIBRARY_IMPL_FRIENDS;

  // Use these functions at your peril.  They are used by the various
  // parsers to set state, and really shouldn't be used by anything else
  // because they don't do the error checking and sending to the
  // network layer that the other setters do.
  void set_encryption(ConnectionSecurity encryption) {
    encryption_ = encryption;
  }
  void set_passphrase(const std::string& passphrase) {
    passphrase_ = passphrase;
    user_passphrase_ = passphrase;
  }
  void set_passphrase_required(bool passphrase_required) {
    passphrase_required_ = passphrase_required;
  }
  void set_identity(const std::string& identity) {
    identity_ = identity;
  }
  void set_eap_method(EAPMethod eap_method) { eap_method_ = eap_method; }
  void set_eap_phase_2_auth(EAPPhase2Auth eap_phase_2_auth) {
    eap_phase_2_auth_ = eap_phase_2_auth;
  }
  void set_eap_server_ca_cert_nss_nickname(
      const std::string& eap_server_ca_cert_nss_nickname) {
    eap_server_ca_cert_nss_nickname_ = eap_server_ca_cert_nss_nickname;
  }
  void set_eap_client_cert_pkcs11_id(
      const std::string& eap_client_cert_pkcs11_id) {
    eap_client_cert_pkcs11_id_ = eap_client_cert_pkcs11_id;
  }
  void set_eap_use_system_cas(bool eap_use_system_cas) {
    eap_use_system_cas_ = eap_use_system_cas;
  }
  void set_eap_identity(const std::string& eap_identity) {
    eap_identity_ = eap_identity;
  }
  void set_eap_anonymous_identity(const std::string& eap_anonymous_identity) {
    eap_anonymous_identity_ = eap_anonymous_identity;
  }
  void set_eap_passphrase(const std::string& eap_passphrase) {
    eap_passphrase_ = eap_passphrase;
  }

  // Network overrides.
  virtual void EraseCredentials() OVERRIDE;
  virtual void CalculateUniqueId() OVERRIDE;

  ConnectionSecurity encryption_;
  std::string passphrase_;
  bool passphrase_required_;
  std::string identity_;

  EAPMethod eap_method_;
  EAPPhase2Auth eap_phase_2_auth_;
  std::string eap_server_ca_cert_nss_nickname_;
  std::string eap_client_cert_pkcs11_id_;
  bool eap_use_system_cas_;
  std::string eap_identity_;
  std::string eap_anonymous_identity_;
  std::string eap_passphrase_;

  // Internal state (not stored in flimflam).
  // Passphrase set by user (stored for UI).
  std::string user_passphrase_;

  DISALLOW_COPY_AND_ASSIGN(WifiNetwork);
};
typedef std::vector<WifiNetwork*> WifiNetworkVector;

// Cellular Data Plan management.
class CellularDataPlan {
 public:
  CellularDataPlan();
  explicit CellularDataPlan(const CellularDataPlanInfo &plan);
  ~CellularDataPlan();

  // Formats cellular plan description.
  string16 GetPlanDesciption() const;
  // Evaluates cellular plans status and returns warning string if it is near
  // expiration.
  string16 GetRemainingWarning() const;
  // Formats remaining plan data description.
  string16 GetDataRemainingDesciption() const;
  // Formats plan expiration description.
  string16 GetPlanExpiration() const;
  // Formats plan usage info.
  string16 GetUsageInfo() const;
  // Returns a unique string for this plan that can be used for comparisons.
  std::string GetUniqueIdentifier() const;
  base::TimeDelta remaining_time() const;
  int64 remaining_minutes() const;
  // Returns plan data remaining in bytes.
  int64 remaining_data() const;
  // TODO(stevenjb): Make these private with accessors and properly named.
  std::string plan_name;
  CellularDataPlanType plan_type;
  base::Time update_time;
  base::Time plan_start_time;
  base::Time plan_end_time;
  int64 plan_data_bytes;
  int64 data_bytes_used;
};
typedef ScopedVector<CellularDataPlan> CellularDataPlanVector;

// Geolocation data.
struct CellTower {
  CellTower();

  enum RadioType {
    RADIOTYPE_GSM,
    RADIOTYPE_CDMA,
    RADIOTYPE_WCDMA,
  } radio_type;                   // GSM/WCDMA     CDMA
  int mobile_country_code;        //   MCC          MCC
  int mobile_network_code;        //   MNC          SID
  int location_area_code;         //   LAC          NID
  int cell_id;                    //   CID          BID
  base::Time timestamp;  // Timestamp when this cell was primary
  int signal_strength;   // Radio signal strength measured in dBm.
  int timing_advance;    // Represents the distance from the cell tower.
                         // Each unit is roughly 550 meters.
};

struct WifiAccessPoint {
  WifiAccessPoint();

  std::string mac_address;  // The mac address of the WiFi node.
  std::string name;         // The SSID of the WiFi node.
  base::Time timestamp;     // Timestamp when this AP was detected.
  int signal_strength;      // Radio signal strength measured in dBm.
  int signal_to_noise;      // Current signal to noise ratio measured in dB.
  int channel;              // Wifi channel number.
};

typedef std::vector<CellTower> CellTowerVector;
typedef std::vector<WifiAccessPoint> WifiAccessPointVector;

// IP Configuration.
struct NetworkIPConfig {
  NetworkIPConfig(const std::string& device_path, IPConfigType type,
                  const std::string& address, const std::string& netmask,
                  const std::string& gateway, const std::string& name_servers);
  ~NetworkIPConfig();

  // Gets the PrefixLength for an IPv4 netmask.
  // For example, "255.255.255.0" => 24
  // If the netmask is invalid, this will return -1;
  // TODO(chocobo): Add support for IPv6.
  int32 GetPrefixLength() const;
  std::string device_path;  // This looks like "/device/0011aa22bb33"
  IPConfigType type;
  std::string address;
  std::string netmask;
  std::string gateway;
  std::string name_servers;
};
typedef std::vector<NetworkIPConfig> NetworkIPConfigVector;

// This class handles the interaction with the ChromeOS network library APIs.
// Classes can add themselves as observers. Users can get an instance of the
// library like this: chromeos::CrosLibrary::Get()->GetNetworkLibrary()
class NetworkLibrary {
 public:
  enum HardwareAddressFormat {
    FORMAT_RAW_HEX,
    FORMAT_COLON_SEPARATED_HEX
  };

  class NetworkManagerObserver {
   public:
    // Called when the state of the network manager has changed,
    // for example, networks have appeared or disappeared.
    virtual void OnNetworkManagerChanged(NetworkLibrary* obj) = 0;
   protected:
    ~NetworkManagerObserver() { }
  };

  class NetworkObserver {
   public:
    // Called when the state of a single network has changed,
    // for example signal strength or connection state.
    virtual void OnNetworkChanged(NetworkLibrary* cros,
                                  const Network* network) = 0;
   protected:
    ~NetworkObserver() {}
  };

  class NetworkDeviceObserver {
   public:
    // Called when the state of a single device has changed.
    virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                        const NetworkDevice* device) {}

    // Called when |device| got notification about new networks available.
    virtual void OnNetworkDeviceFoundNetworks(NetworkLibrary* cros,
                                              const NetworkDevice* device) {}

    // Called when |device| got notification about SIM lock change.
    virtual void OnNetworkDeviceSimLockChanged(NetworkLibrary* cros,
                                               const NetworkDevice* device) {}
   protected:
    ~NetworkDeviceObserver() {}
  };

  class CellularDataPlanObserver {
   public:
    // Called when the cellular data plan has changed.
    virtual void OnCellularDataPlanChanged(NetworkLibrary* obj) = 0;
   protected:
    ~CellularDataPlanObserver() {}
  };

  class PinOperationObserver {
   public:
    // Called when pin async operation has completed.
    // Network is NULL when we don't have an associated Network object.
    virtual void OnPinOperationCompleted(NetworkLibrary* cros,
                                         PinOperationError error) = 0;
   protected:
    ~PinOperationObserver() {}
  };

  class UserActionObserver {
   public:
    // Called when user initiates a new connection.
    // Network is NULL when we don't have an associated Network object.
    virtual void OnConnectionInitiated(NetworkLibrary* cros,
                                       const Network* network) = 0;
   protected:
    ~UserActionObserver() {}
  };

  virtual ~NetworkLibrary() {}

  virtual void Init() = 0;

  // Returns true if libcros was loaded instead of stubbed out.
  virtual bool IsCros() const = 0;

  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) = 0;
  virtual void RemoveNetworkManagerObserver(
      NetworkManagerObserver* observer) = 0;

  // An attempt to add an observer that has already been added for a
  // give service path will be ignored.
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) = 0;
  // Remove an observer of a single network
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) = 0;
  // Stop |observer| from observing any networks
  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) = 0;

  // Add an observer for a single network device.
  virtual void AddNetworkDeviceObserver(const std::string& device_path,
                                        NetworkDeviceObserver* observer) = 0;
  // Remove an observer for a single network device.
  virtual void RemoveNetworkDeviceObserver(const std::string& device_path,
                                           NetworkDeviceObserver* observer) = 0;

  // Temporarily locks down certain functionality in network library to prevent
  // unplanned side effects. During the lock down, Enable*Device() calls cannot
  // be made.
  virtual void Lock() = 0;
  // Removes temporarily lock of network library.
  virtual void Unlock() = 0;
  // Checks if access to network library is locked.
  virtual bool IsLocked() = 0;

  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) = 0;
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) = 0;

  virtual void AddPinOperationObserver(PinOperationObserver* observer) = 0;
  virtual void RemovePinOperationObserver(PinOperationObserver* observer) = 0;

  virtual void AddUserActionObserver(UserActionObserver* observer) = 0;
  virtual void RemoveUserActionObserver(UserActionObserver* observer) = 0;

  // Return the active or default Ethernet network (or NULL if none).
  virtual const EthernetNetwork* ethernet_network() const = 0;
  virtual bool ethernet_connecting() const = 0;
  virtual bool ethernet_connected() const = 0;

  // Return the active Wifi network (or NULL if none active).
  virtual const WifiNetwork* wifi_network() const = 0;
  virtual bool wifi_connecting() const = 0;
  virtual bool wifi_connected() const = 0;

  // Return the active Cellular network (or NULL if none active).
  virtual const CellularNetwork* cellular_network() const = 0;
  virtual bool cellular_connecting() const = 0;
  virtual bool cellular_connected() const = 0;

  // Return the active virtual network (or NULL if none active).
  virtual const VirtualNetwork* virtual_network() const = 0;
  virtual bool virtual_network_connecting() const = 0;
  virtual bool virtual_network_connected() const = 0;

  // Return true if any network is currently connected.
  virtual bool Connected() const = 0;

  // Return true if any network is currently connecting.
  virtual bool Connecting() const = 0;

  // Returns the current list of wifi networks.
  virtual const WifiNetworkVector& wifi_networks() const = 0;

  // Returns the list of remembered wifi networks.
  virtual const WifiNetworkVector& remembered_wifi_networks() const = 0;

  // Returns the current list of cellular networks.
  virtual const CellularNetworkVector& cellular_networks() const = 0;

  // Returns the current list of virtual networks.
  virtual const VirtualNetworkVector& virtual_networks() const = 0;

  // Returns the current list of virtual networks.
  virtual const VirtualNetworkVector& remembered_virtual_networks() const = 0;

  virtual const Network* active_network() const = 0;
  virtual const Network* connected_network() const = 0;
  virtual const Network* connecting_network() const = 0;

  virtual bool ethernet_available() const = 0;
  virtual bool wifi_available() const = 0;
  virtual bool cellular_available() const = 0;

  virtual bool ethernet_enabled() const = 0;
  virtual bool wifi_enabled() const = 0;
  virtual bool cellular_enabled() const = 0;

  virtual bool ethernet_busy() const = 0;
  virtual bool wifi_busy() const = 0;
  virtual bool cellular_busy() const = 0;

  virtual bool wifi_scanning() const = 0;

  virtual bool offline_mode() const = 0;

  // Returns the current IP address if connected. If not, returns empty string.
  virtual const std::string& IPAddress() const = 0;

  // Return a pointer to the device, if it exists, or NULL.
  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const = 0;

  // Returns device with TYPE_CELLULAR. Returns NULL if none exists.
  virtual const NetworkDevice* FindCellularDevice() const = 0;

  // Returns device with TYPE_ETHERNET. Returns NULL if none exists.
  virtual const NetworkDevice* FindEthernetDevice() const = 0;

  // Returns device with TYPE_WIFI. Returns NULL if none exists.
  virtual const NetworkDevice* FindWifiDevice() const = 0;

  // Return a pointer to the network, if it exists, or NULL.
  // NOTE: Never store these results, store service paths instead.
  // The pattern for doing an operation on a Network is:
  // Network* network = cros->FindNetworkByPath(service_path);
  // network->SetFoo();
  // network->Connect();
  // As long as this is done in sequence on the UI thread it will be safe;
  // the network list only gets updated on the UI thread.
  virtual Network* FindNetworkByPath(const std::string& path) const = 0;
  virtual Network* FindNetworkByUniqueId(
      const std::string& unique_id) const = 0;
  virtual WifiNetwork* FindWifiNetworkByPath(const std::string& path) const = 0;
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const = 0;
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const = 0;

  // Return a pointer to the remembered network, if it exists, or NULL.
  virtual Network* FindRememberedNetworkByPath(
      const std::string& path) const = 0;
  virtual Network* FindRememberedNetworkByUniqueId(
      const std::string& unique_id) const = 0;

  // Retrieves the data plans associated with |path|, NULL if there are no
  // associated plans.
  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const = 0;

  // This returns the significant data plan. If the user only has the
  // base data plan, then return that. If there is a base and a paid data plan,
  // then the significant one is the paid one. So return the paid plan.
  // If there are no data plans, then this method returns NULL.
  // This returns a pointer to a member of data_plans_, so if SetDataPlans()
  // gets called, the result becomes invalid.
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const = 0;

  // Records information that cellular plan payment has happened.
  virtual void SignalCellularPlanPayment() = 0;

  // Returns true if cellular plan payment has been recorded recently.
  virtual bool HasRecentCellularPlanPayment() = 0;

  // Returns home carrier ID if available, otherwise empty string is returned.
  // Carrier ID format: <carrier name> (country). Ex.: "Verizon (us)".
  virtual std::string GetCellularHomeCarrierId() const = 0;

  // Passes |old_pin|, |new_pin| to change SIM card PIM.
  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) = 0;

  // Passes |pin|, |require_pin| value to change SIM card RequirePin setting.
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) = 0;

  // Passes |pin| to unlock SIM card.
  virtual void EnterPin(const std::string& pin) = 0;

  // Passes |puk|, |new_pin| to unblock SIM card.
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) = 0;

  // Request a scan for available cellular networks.
  virtual void RequestCellularScan() = 0;

  // Request a register in cellular network with |network_id|.
  virtual void RequestCellularRegister(const std::string& network_id) = 0;

  // Change data roaming restriction for current cellular device.
  virtual void SetCellularDataRoamingAllowed(bool new_value) = 0;

  // Return true if GSM SIM card can work only with enabled roaming.
  virtual bool IsCellularAlwaysInRoaming() = 0;

  // Request a scan for new wifi networks.
  virtual void RequestNetworkScan() = 0;

  // Reads out the results of the last wifi scan. These results are not
  // pre-cached in the library, so the call may block whilst the results are
  // read over IPC.
  // Returns false if an error occurred in reading the results. Note that
  // a true return code only indicates the result set was successfully read,
  // it does not imply a scan has successfully completed yet.
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) = 0;

  // TODO(joth): Add GetCellTowers to retrieve a CellTowerVector.

  // Return true if a profile matching |type| is loaded.
  virtual bool HasProfileType(NetworkProfileType type) const = 0;

  // Move the network to the shared/global profile.
  virtual void SetNetworkProfile(const std::string& service_path,
                                 NetworkProfileType type) = 0;

  // Returns false if there is no way to connect to this network, even with
  // user input (e.g. it requires a user profile but none is available).
  virtual bool CanConnectToNetwork(const Network* network) const = 0;

  // Connect to the specified wireless network.
  virtual void ConnectToWifiNetwork(WifiNetwork* network) = 0;

  // Connect to the specified wireless network and set its profile
  // to SHARED if |shared| is true, otherwise to USER.
  virtual void ConnectToWifiNetwork(WifiNetwork* network, bool shared) = 0;

  // Connect to the specified cellular network.
  virtual void ConnectToCellularNetwork(CellularNetwork* network) = 0;

  // Connect to the specified virtual network.
  virtual void ConnectToVirtualNetwork(VirtualNetwork* network) = 0;

  // Connect to an unconfigured network with given SSID, security, passphrase,
  // and optional EAP configuration. If |security| is SECURITY_8021X,
  // |eap_config| must be provided.
  struct EAPConfigData {
    EAPConfigData()
        : method(EAP_METHOD_UNKNOWN),
          auth(EAP_PHASE_2_AUTH_AUTO),
          use_system_cas(true) {}
    ~EAPConfigData() {}
    EAPMethod method;
    EAPPhase2Auth auth;
    std::string server_ca_cert_nss_nickname;
    bool use_system_cas;
    std::string client_cert_pkcs11_id;
    std::string identity;
    std::string anonymous_identity;
  };
  virtual void ConnectToUnconfiguredWifiNetwork(
      const std::string& ssid,
      ConnectionSecurity security,
      const std::string& passphrase,
      const EAPConfigData* eap_config,
      bool save_credentials,
      bool shared) = 0;

  // Connect to the specified virtual network with service name.
  // VPNConfigData must be provided.
  struct VPNConfigData {
    VPNConfigData() {}
    ~VPNConfigData() {}
    std::string psk;
    std::string server_ca_cert_nss_nickname;
    std::string client_cert_pkcs11_id;
    std::string username;
    std::string user_passphrase;
    std::string otp;
    std::string group_name;
  };
  virtual void ConnectToUnconfiguredVirtualNetwork(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type,
      const VPNConfigData& config) = 0;

  // Disconnect from the specified network.
  virtual void DisconnectFromNetwork(const Network* network) = 0;

  // Forget the network corresponding to service_path.
  virtual void ForgetNetwork(const std::string& service_path) = 0;

  // Enables/disables the ethernet network device.
  virtual void EnableEthernetNetworkDevice(bool enable) = 0;

  // Enables/disables the wifi network device.
  virtual void EnableWifiNetworkDevice(bool enable) = 0;

  // Enables/disables the cellular network device.
  virtual void EnableCellularNetworkDevice(bool enable) = 0;

  // Enables/disables offline mode.
  virtual void EnableOfflineMode(bool enable) = 0;

  // Fetches IP configs and hardware address for a given device_path.
  // The hardware address is usually a MAC address like "0011AA22BB33".
  // |hardware_address| will be an empty string, if no hardware address is
  // found.
  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat) = 0;

  // Sets an IP config. This is called when user changes from dhcp to static
  // or vice versa or when user changes the ip config info.
  // If nothing is changed, this method does nothing.
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) = 0;

  // This will connect to a preferred network if the currently connected
  // network is not preferred. This should be called when the active profile
  // changes.
  virtual void SwitchToPreferredNetwork() = 0;

  // Load networks from an Open Network Configuration blob.
  virtual bool LoadOncNetworks(const std::string& onc_blob) = 0;

  // This sets the active network for the network type. Note: priority order
  // is unchanged (i.e. if a wifi network is set to active, but an ethernet
  // network is still active, active_network() will still return the ethernet
  // network). Other networks of the same type will become inactive.
  // Used for testing.
  virtual bool SetActiveNetwork(ConnectionType type,
                                const std::string& service_path) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static NetworkLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
