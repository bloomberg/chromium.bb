// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/timer.h"
#include "third_party/cros/chromeos_network.h"

class DictionaryValue;
class Value;

namespace chromeos {

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
  STATE_ACTIVATION_FAILURE = 8
};

enum ConnectivityState {
  CONN_STATE_UNKNOWN      = 0,
  CONN_STATE_UNRESTRICTED = 1,
  CONN_STATE_RESTRICTED   = 2,
  CONN_STATE_NONE         = 3
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

// SIMLock states (see gobi-cromo-plugin/gobi_gsm_modem.cc)
enum SIMLockState {
  SIM_UNKNOWN    = 0,
  SIM_UNLOCKED   = 1,
  SIM_LOCKED_PIN = 2,
  SIM_LOCKED_PUK = 3,  // also when SIM is blocked, then retries = 0.
};

// SIM PinRequire states. Since PinRequire current state is not exposed as a
// cellular property, we initialize it's value based on the SIMLockState
// initial value.
// SIM_PIN_REQUIRE_UNKNOWN - SIM card is absent or SIMLockState initial value
//                           hasn't been received yet.
// SIM_PIN_REQUIRED - SIM card is locked when booted/wake from sleep and
//                    requires user to enter PIN.
// SIM_PIN_NOT_REQUIRED - SIM card is unlocked all the time and requires PIN
// only on certain operations, such as ChangeRequirePin, ChangePin, EnterPin.
enum SIMPinRequire {
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
  ERROR_UNKNOWN           = 0,
  ERROR_OUT_OF_RANGE      = 1,
  ERROR_PIN_MISSING       = 2,
  ERROR_DHCP_FAILED       = 3,
  ERROR_CONNECT_FAILED    = 4,
  ERROR_BAD_PASSPHRASE    = 5,
  ERROR_BAD_WEPKEY        = 6,
  ERROR_ACTIVATION_FAILED = 7,
  ERROR_NEED_EVDO         = 8,
  ERROR_NEED_HOME_NETWORK = 9,
  ERROR_OTASP_FAILED      = 10,
  ERROR_AAA_FAILED        = 11,
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

// Cellular network is considered low data when less than 60 minues.
static const int kCellularDataLowSecs = 60 * 60;

// Cellular network is considered low data when less than 30 minues.
static const int kCellularDataVeryLowSecs = 30 * 60;

// Cellular network is considered low data when less than 100MB.
static const int kCellularDataLowBytes = 100 * 1024 * 1024;

// Cellular network is considered very low data when less than 50MB.
static const int kCellularDataVeryLowBytes = 50 * 1024 * 1024;

// Contains data related to the flimflam.Device interface,
// e.g. ethernet, wifi, cellular.
// TODO(dpolukhin): refactor to make base class and device specific derivatives.
class NetworkDevice {
 public:
  explicit NetworkDevice(const std::string& device_path);
  ~NetworkDevice();

  // Device info.
  const std::string& device_path() const { return device_path_; }
  const std::string& name() const { return name_; }
  ConnectionType type() const { return type_; }
  bool scanning() const { return scanning_; }
  const std::string& meid() const { return MEID_; }
  const std::string& imei() const { return IMEI_; }
  const std::string& imsi() const { return IMSI_; }
  const std::string& esn() const { return ESN_; }
  const std::string& mdn() const { return MDN_; }
  const std::string& min() const { return MIN_; }
  const std::string& model_id() const { return model_id_; }
  const std::string& manufacturer() const { return manufacturer_; }
  SIMLockState sim_lock_state() const { return sim_lock_state_; }
  const int sim_retries_left() const { return sim_retries_left_; }
  SIMPinRequire sim_pin_required() const { return sim_pin_required_; }
  const std::string& firmware_revision() const { return firmware_revision_; }
  const std::string& hardware_revision() const { return hardware_revision_; }
  const unsigned int prl_version() const { return PRL_version_; }
  const std::string& home_provider() const { return home_provider_; }
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

 private:
  bool ParseValue(int index, const Value* value);
  void ParseInfo(const DictionaryValue* info);

  // General device info.
  std::string device_path_;
  std::string name_;
  ConnectionType type_;
  bool scanning_;
  // Cellular specific device info.
  std::string carrier_;
  std::string home_provider_;
  std::string home_provider_code_;
  std::string home_provider_country_;
  std::string home_provider_id_;
  std::string home_provider_name_;
  std::string MEID_;
  std::string IMEI_;
  std::string IMSI_;
  std::string ESN_;
  std::string MDN_;
  std::string MIN_;
  std::string model_id_;
  std::string manufacturer_;
  SIMLockState sim_lock_state_;
  int sim_retries_left_;
  SIMPinRequire sim_pin_required_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  int PRL_version_;
  std::string selected_cellular_network_;
  CellularNetworkList found_cellular_networks_;
  bool data_roaming_allowed_;
  bool support_network_scan_;

  friend class NetworkLibraryImpl;
  DISALLOW_COPY_AND_ASSIGN(NetworkDevice);
};

// Contains data common to all network service types.
class Network {
 public:
  virtual ~Network();

  const std::string& service_path() const { return service_path_; }
  const std::string& name() const { return name_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& ip_address() const { return ip_address_; }
  ConnectionType type() const { return type_; }
  ConnectionMode mode() const { return mode_; }
  ConnectionState connection_state() const { return state_; }
  bool connecting() const { return IsConnectingState(state_); }
  bool configuring() const { return state_ == STATE_CONFIGURATION; }
  bool connected() const { return state_ == STATE_READY; }
  bool connecting_or_connected() const { return connecting() || connected(); }
  bool failed() const { return state_ == STATE_FAILURE; }
  bool failed_or_disconnected() const {
    return failed() || state_ == STATE_IDLE;
  }
  ConnectionError error() const { return error_; }
  ConnectionState state() const { return state_; }
  // Is this network connectable. Currently, this is mainly used by 802.1x
  // networks to specify that the network is not configured yet.
  bool connectable() const { return connectable_; }
  // Is this the active network, i.e, the one through which
  // network traffic is being routed? A network can be connected,
  // but not be carrying traffic.
  bool is_active() const { return is_active_; }
  bool favorite() const { return favorite_; }
  bool auto_connect() const { return auto_connect_; }
  ConnectivityState connectivity_state() const { return connectivity_state_; }
  bool added() const { return added_; }
  bool notify_failure() const { return notify_failure_; }

  const std::string& unique_id() const { return unique_id_; }

  void set_notify_failure(bool state) { notify_failure_ = state; }

  // We don't have a setter for |favorite_| because to unfavorite a network is
  // equivalent to forget a network, so we call forget network on cros for
  // that.  See ForgetWifiNetwork().
  void SetAutoConnect(bool auto_connect);

  // Sets network name.
  void SetName(const std::string& name);

  // Return a string representation of the state code.
  std::string GetStateString() const;

  // Return a string representation of the error code.
  std::string GetErrorString() const;

  static bool IsConnectingState(ConnectionState state) {
    return (state == STATE_ASSOCIATION ||
            state == STATE_CONFIGURATION ||
            state == STATE_CARRIER);
  }

 protected:
  Network(const std::string& service_path, ConnectionType type);

  // Parse name/value pairs from libcros.
  virtual bool ParseValue(int index, const Value* value);
  virtual void ParseInfo(const DictionaryValue* info);

  // Methods to asynchronously set network service properties
  virtual void SetStringProperty(const char* prop, const std::string& str,
                                 std::string* dest);
  virtual void SetBooleanProperty(const char* prop, bool b, bool* dest);
  virtual void SetIntegerProperty(const char* prop, int i, int* dest);
  virtual void SetValueProperty(const char* prop, Value* val);
  virtual void ClearProperty(const char* prop);

  // This will clear the property if string is empty. Otherwise, it will set it.
  virtual void SetOrClearStringProperty(const char* prop,
                                        const std::string& str,
                                        std::string* dest);

  std::string device_path_;
  std::string name_;
  std::string ip_address_;
  ConnectionMode mode_;
  ConnectionState state_;
  ConnectionError error_;
  bool connectable_;
  bool is_active_;
  bool favorite_;
  bool auto_connect_;
  ConnectivityState connectivity_state_;

  // Unique identifier, set the first time the network is parsed.
  std::string unique_id_;

 private:
  void set_name(const std::string& name) { name_ = name; }
  void set_connecting(bool connecting) {
    state_ = (connecting ? STATE_ASSOCIATION : STATE_IDLE);
  }
  void set_connected(bool connected) {
    state_ = (connected ? STATE_READY : STATE_IDLE);
  }
  void set_connectable(bool connectable) { connectable_ = connectable; }
  void set_active(bool is_active) { is_active_ = is_active; }
  void set_error(ConnectionError error) { error_ = error; }
  void set_connectivity_state(ConnectivityState connectivity_state) {
    connectivity_state_ = connectivity_state;
  }
  void set_added(bool added) { added_ = added; }

  // Set the state and update flags if necessary.
  void SetState(ConnectionState state);

  // Initialize the IP address field
  void InitIPAddress();

  // Priority value, corresponds to index in list from flimflam (0 = first)
  int priority_order_;

  // Set to true if the UI requested this as a new network.
  bool added_;

  // Set to true when a new connection failure occurs; cleared when observers
  // are notified.
  bool notify_failure_;

  // These must not be modified after construction.
  std::string service_path_;
  ConnectionType type_;

  friend class NetworkLibraryImpl;
  friend class NetworkLibraryStubImpl;
  DISALLOW_COPY_AND_ASSIGN(Network);
  // ChangeAutoConnectSaveTest accesses |favorite_|.
  FRIEND_TEST_ALL_PREFIXES(WifiConfigViewTest, ChangeAutoConnectSaveTest);
};

// Class for networks of TYPE_ETHERNET.
class EthernetNetwork : public Network {
 public:
  explicit EthernetNetwork(const std::string& service_path) :
      Network(service_path, TYPE_ETHERNET) {
  }
 private:
  friend class NetworkLibraryImpl;
  DISALLOW_COPY_AND_ASSIGN(EthernetNetwork);
};

// Class for networks of TYPE_VPN.
class VirtualNetwork : public Network {
 public:
  enum ProviderType {
    PROVIDER_TYPE_L2TP_IPSEC_PSK,
    PROVIDER_TYPE_L2TP_IPSEC_USER_CERT,
    PROVIDER_TYPE_OPEN_VPN,
    // Add new provider types before PROVIDER_TYPE_MAX.
    PROVIDER_TYPE_MAX,
  };

  explicit VirtualNetwork(const std::string& service_path);
  ~VirtualNetwork();

  const std::string& server_hostname() const { return server_hostname_; }
  ProviderType provider_type() const { return provider_type_; }
  const std::string& ca_cert_nss() const { return ca_cert_nss_; }
  const std::string& psk_passphrase() const { return psk_passphrase_; }
  const std::string& client_cert_id() const { return client_cert_id_; }
  const std::string& username() const { return username_; }
  const std::string& user_passphrase() const { return user_passphrase_; }

  bool NeedMoreInfoToConnect() const;

  // Public setters.
  void SetCACertNSS(const std::string& ca_cert_nss);
  void SetPSKPassphrase(const std::string& psk_passphrase);
  void SetClientCertID(const std::string& cert_id);
  void SetUsername(const std::string& username);
  void SetUserPassphrase(const std::string& user_passphrase);

  std::string GetProviderTypeString() const;

 private:
  // Network overrides.
  virtual bool ParseValue(int index, const Value* value);
  virtual void ParseInfo(const DictionaryValue* info);

  // VirtualNetwork private methods.
  bool ParseProviderValue(int index, const Value* value);

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
  void set_username(const std::string& username) {
    username_ = username;
  }
  void set_user_passphrase(const std::string& user_passphrase) {
    user_passphrase_ = user_passphrase;
  }

  std::string server_hostname_;
  ProviderType provider_type_;
  // NSS nickname for server CA certificate.
  std::string ca_cert_nss_;
  std::string psk_passphrase_;
  // PKCS#11 ID for client certificate.
  std::string client_cert_id_;
  std::string username_;
  std::string user_passphrase_;

  friend class NetworkLibraryImpl;
  DISALLOW_COPY_AND_ASSIGN(VirtualNetwork);
};
typedef std::vector<VirtualNetwork*> VirtualNetworkVector;

// Base class for networks of TYPE_WIFI or TYPE_CELLULAR.
class WirelessNetwork : public Network {
 public:
  int strength() const { return strength_; }

 protected:
  WirelessNetwork(const std::string& service_path, ConnectionType type)
      : Network(service_path, type),
        strength_(0) {}
  int strength_;

  // Network overrides.
  virtual bool ParseValue(int index, const Value* value);

 private:
  void set_strength(int strength) { strength_ = strength; }

  friend class NetworkLibraryImpl;
  friend class NetworkLibraryStubImpl;
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

  struct Apn {
    std::string apn;
    std::string network_id;
    std::string username;
    std::string password;

    Apn();
    Apn(const std::string& apn, const std::string& network_id,
        const std::string& username, const std::string& password);
    ~Apn();
    void Set(const DictionaryValue& dict);
  };

  explicit CellularNetwork(const std::string& service_path);
  virtual ~CellularNetwork();

  // Starts device activation process. Returns false if the device state does
  // not permit activation.
  bool StartActivation() const;
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
  bool restricted_pool() const {
    return connectivity_state() == CONN_STATE_RESTRICTED;
  }
  bool needs_new_plan() const {
    return restricted_pool() && connected() && activated();
  }
  const std::string& operator_name() const { return operator_name_; }
  const std::string& operator_code() const { return operator_code_; }
  const std::string& operator_country() const { return operator_country_; }
  const std::string& payment_url() const { return payment_url_; }
  const std::string& usage_url() const { return usage_url_; }
  DataLeft data_left() const { return data_left_; }
  const Apn& apn() const { return apn_; }
  const Apn& last_good_apn() const { return last_good_apn_; }
  void SetApn(const Apn& apn);
  bool SupportsDataPlan() const;

  // Misc.
  bool is_gsm() const {
    return network_technology_ != NETWORK_TECHNOLOGY_EVDO &&
        network_technology_ != NETWORK_TECHNOLOGY_1XRTT &&
        network_technology_ != NETWORK_TECHNOLOGY_UNKNOWN;
  }

  // Return a string representation of network technology.
  std::string GetNetworkTechnologyString() const;
  // Return a string representation of connectivity state.
  std::string GetConnectivityStateString() const;
  // Return a string representation of activation state.
  std::string GetActivationStateString() const;
  // Return a string representation of roaming state.
  std::string GetRoamingStateString() const;

  // Return a string representation of |activation_state|.
  static std::string ActivationStateToString(ActivationState activation_state);

 protected:
  // WirelessNetwork overrides.
  virtual bool ParseValue(int index, const Value* value);

  ActivationState activation_state_;
  NetworkTechnology network_technology_;
  NetworkRoamingState roaming_state_;
  // Carrier Info
  std::string operator_name_;
  std::string operator_code_;
  std::string operator_country_;
  std::string payment_url_;
  std::string usage_url_;
  // Cached values
  DataLeft data_left_;  // Updated when data plans are updated.
  Apn apn_;
  Apn last_good_apn_;

 private:
  void set_activation_state(ActivationState state) {
    activation_state_ = state;
  }
  void set_payment_url(const std::string& url) { payment_url_ = url; }
  void set_usage_url(const std::string& url) { usage_url_ = url; }
  void set_network_technology(NetworkTechnology technology) {
    network_technology_ = technology;
  }
  void set_roaming_state(NetworkRoamingState state) { roaming_state_ = state; }
  void set_data_left(DataLeft data_left) { data_left_ = data_left; }
  void set_apn(const Apn& apn) { apn_ = apn; }
  void set_last_good_apn(const Apn& apn) { last_good_apn_ = apn; }

  friend class NetworkLibraryImpl;
  friend class NetworkLibraryStubImpl;
  DISALLOW_COPY_AND_ASSIGN(CellularNetwork);
};
typedef std::vector<CellularNetwork*> CellularNetworkVector;

// Class for networks of TYPE_WIFI.
class WifiNetwork : public WirelessNetwork {
 public:
  explicit WifiNetwork(const std::string& service_path);
  virtual ~WifiNetwork();

  bool encrypted() const { return encryption_ != SECURITY_NONE; }
  ConnectionSecurity encryption() const { return encryption_; }
  const std::string& passphrase() const { return passphrase_; }
  const std::string& identity() const { return identity_; }
  const std::string& cert_path() const { return cert_path_; }
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
  bool save_credentials() const { return save_credentials_; }

  const std::string& GetPassphrase() const;

  bool SetSsid(const std::string& ssid);
  bool SetHexSsid(const std::string& ssid_hex);
  void SetPassphrase(const std::string& passphrase);
  void SetIdentity(const std::string& identity);
  void SetCertPath(const std::string& cert_path);

  // 802.1x properties
  void SetEAPMethod(EAPMethod method);
  void SetEAPPhase2Auth(EAPPhase2Auth auth);
  void SetEAPServerCaCertNssNickname(const std::string& nss_nickname);
  void SetEAPClientCertPkcs11Id(const std::string& pkcs11_id);
  void SetEAPUseSystemCAs(bool use_system_cas);
  void SetEAPIdentity(const std::string& identity);
  void SetEAPAnonymousIdentity(const std::string& identity);
  void SetEAPPassphrase(const std::string& passphrase);
  void SetSaveCredentials(bool save_credentials);

  // Erase cached credentials, used when "Save password" is unchecked.
  void EraseCredentials();

  // Return a string representation of the encryption code.
  // This not translated and should be only used for debugging purposes.
  std::string GetEncryptionString() const;

  // Return true if a passphrase or other input is required to connect.
  bool IsPassphraseRequired() const;

  // Return true if cert_path_ indicates that we have loaded the certificate.
  bool IsCertificateLoaded() const;

 private:
  // WirelessNetwork overrides.
  virtual bool ParseValue(int index, const Value* value);
  virtual void ParseInfo(const DictionaryValue* info);

  void CalculateUniqueId();

  void set_encryption(ConnectionSecurity encryption) {
    encryption_ = encryption;
  }
  void set_passphrase(const std::string& passphrase) {
    passphrase_ = passphrase;
  }
  void set_passphrase_required(bool passphrase_required) {
    passphrase_required_ = passphrase_required;
  }
  void set_identity(const std::string& identity) {
    identity_ = identity;
  }
  void set_cert_path(const std::string& cert_path) {
    cert_path_ = cert_path;
  }

  ConnectionSecurity encryption_;
  std::string passphrase_;
  bool passphrase_required_;
  std::string identity_;
  std::string cert_path_;

  EAPMethod eap_method_;
  EAPPhase2Auth eap_phase_2_auth_;
  std::string eap_server_ca_cert_nss_nickname_;
  std::string eap_client_cert_pkcs11_id_;
  bool eap_use_system_cas_;
  std::string eap_identity_;
  std::string eap_anonymous_identity_;
  std::string eap_passphrase_;
  // Tells flimflam to save passphrase and EAP credentials to disk.
  bool save_credentials_;

  // Internal state (not stored in flimflam).
  // Passphrase set by user (stored for UI).
  std::string user_passphrase_;

  friend class NetworkLibraryImpl;
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

  // NetworkIPConfigs are sorted by tyoe.
  bool operator< (const NetworkIPConfig& other) const {
    return type < other.type;
  }

  std::string device_path;
  IPConfigType type;
  std::string address;  // This looks like "/device/0011aa22bb33"
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
  };

  class NetworkObserver {
   public:
    // Called when the state of a single network has changed,
    // for example signal strength or connection state.
    virtual void OnNetworkChanged(NetworkLibrary* cros,
                                  const Network* network) = 0;
  };

  class NetworkDeviceObserver {
   public:
    // Called when the state of a single device has changed,
    // for example SIMLock state for cellular.
    virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                        const NetworkDevice* device) = 0;
  };

  class CellularDataPlanObserver {
   public:
    // Called when the cellular data plan has changed.
    virtual void OnCellularDataPlanChanged(NetworkLibrary* obj) = 0;
  };

  class PinOperationObserver {
   public:
    // Called when pin async operation has completed.
    // Network is NULL when we don't have an associated Network object.
    virtual void OnPinOperationCompleted(NetworkLibrary* cros,
                                         PinOperationError error) = 0;
  };

  class UserActionObserver {
   public:
    // Called when user initiates a new connection.
    // Network is NULL when we don't have an associated Network object.
    virtual void OnConnectionInitiated(NetworkLibrary* cros,
                                       const Network* network) = 0;
  };

  virtual ~NetworkLibrary() {}

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

  // Returns the current IP address if connected. If not, returns empty string.
  virtual const std::string& IPAddress() const = 0;

  // Returns the current list of wifi networks.
  virtual const WifiNetworkVector& wifi_networks() const = 0;

  // Returns the list of remembered wifi networks.
  virtual const WifiNetworkVector& remembered_wifi_networks() const = 0;

  // Returns the current list of cellular networks.
  virtual const CellularNetworkVector& cellular_networks() const = 0;

  // Returns the current list of virtual networks.
  virtual const VirtualNetworkVector& virtual_networks() const = 0;

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
  virtual WifiNetwork* FindWifiNetworkByPath(const std::string& path) const = 0;
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const = 0;
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const = 0;

  // Returns the visible wifi network corresponding to the remembered
  // wifi network, or NULL if the remembered network is not visible.
  virtual Network* FindNetworkFromRemembered(
      const Network* remembered) const = 0;

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

  // Connect to the specified wireless network.
  virtual void ConnectToWifiNetwork(WifiNetwork* network) = 0;

  // Same as above but searches for an existing network by name.
  virtual void ConnectToWifiNetwork(const std::string& service_path) = 0;

  // Connect to a hidden network with given SSID, security, and passphrase.
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    ConnectionSecurity security,
                                    const std::string& passphrase) = 0;

  // Connect to a hidden 802.1X network.
  virtual void ConnectToWifiNetwork8021x(
      const std::string& ssid,
      EAPMethod method,
      EAPPhase2Auth auth,
      const std::string& server_ca_cert_nss_nickname,
      bool use_system_cas,
      const std::string& client_cert_pkcs11_id,
      const std::string& identity,
      const std::string& anonymous_identity,
      const std::string& passphrase,
      bool save_credentials) = 0;

  // Connect to the specified cellular network.
  virtual void ConnectToCellularNetwork(CellularNetwork* network) = 0;

  // Records information that cellular play payment had happened.
  virtual void SignalCellularPlanPayment() = 0;

  // Returns true if cellular plan payment had been recorded recently.
  virtual bool HasRecentCellularPlanPayment() = 0;

  // Connect to the specified virtual network.
  virtual void ConnectToVirtualNetwork(VirtualNetwork* network) = 0;

  // Connect to the specified virtual network with service name,
  // server hostname, provider_type, PSK passphrase, username and passphrase.
  virtual void ConnectToVirtualNetworkPSK(
      const std::string& service_name,
      const std::string& server_hostname,
      const std::string& psk,
      const std::string& username,
      const std::string& user_passphrase) = 0;

  // Connect to a virtual network with user certificate information.
  // TODO(jamescook): Convert both this and above to take a struct of
  // configuration information.
  virtual void ConnectToVirtualNetworkCert(
      const std::string& service_name,
      const std::string& server_hostname,
      const std::string& client_cert_id,
      const std::string& username,
      const std::string& user_passphrase) = 0;

  // Disconnect from the specified network.
  virtual void DisconnectFromNetwork(const Network* network) = 0;

  // Forget the wifi network corresponding to service_path.
  virtual void ForgetWifiNetwork(const std::string& service_path) = 0;

  // Returns home carrier ID if available, otherwise empty string is returned.
  // Carrier ID format: <carrier name> (country). Ex.: "Verizon (us)".
  virtual std::string GetCellularHomeCarrierId() const = 0;

  virtual bool ethernet_available() const = 0;
  virtual bool wifi_available() const = 0;
  virtual bool cellular_available() const = 0;

  virtual bool ethernet_enabled() const = 0;
  virtual bool wifi_enabled() const = 0;
  virtual bool cellular_enabled() const = 0;

  virtual bool wifi_scanning() const = 0;

  virtual const Network* active_network() const = 0;
  virtual const Network* connected_network() const = 0;

  virtual bool offline_mode() const = 0;

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

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static NetworkLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
