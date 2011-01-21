  // Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <algorithm>
#include <map>

#include "base/i18n/time_formatting.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/network_login_observer.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// FlimFlam may send multiple notifications for single network change.
// We wait small amount of time before retrieving the status to
// avoid send multiple sync request to flim flam.
const int kNetworkUpdateDelayMs = 50;

}  // namespace

namespace chromeos {

namespace {
// TODO(ers) These string constants and Parse functions are copied
// straight out of libcros:chromeos_network.cc. Fix this by moving
// all handling of properties into libcros.
// Network service properties we are interested in monitoring
static const char* kConnectableProperty = "Connectable";
static const char* kIsActiveProperty = "IsActive";
static const char* kStateProperty = "State";
static const char* kConnectivityStateProperty = "ConnectivityState";
static const char* kSignalStrengthProperty = "Strength";
static const char* kActivationStateProperty = "Cellular.ActivationState";
static const char* kNetworkTechnologyProperty = "Cellular.NetworkTechnology";
static const char* kPaymentURLProperty = "Cellular.OlpUrl";
  static const char* kRoamingStateProperty = "Cellular.RoamingState";

// Connman state options.
static const char* kStateIdle = "idle";
static const char* kStateCarrier = "carrier";
static const char* kStateAssociation = "association";
static const char* kStateConfiguration = "configuration";
static const char* kStateReady = "ready";
  static const char* kStateDisconnect = "disconnect";
static const char* kStateFailure = "failure";
static const char* kStateActivationFailure = "activation-failure";

// Connman activation state options
static const char* kActivationStateActivated = "activated";
static const char* kActivationStateActivating = "activating";
static const char* kActivationStateNotActivated = "not-activated";
static const char* kActivationStatePartiallyActivated = "partially-activated";
static const char* kActivationStateUnknown = "unknown";

// Connman connectivity state options
static const char* kConnStateUnrestricted = "unrestricted";
static const char* kConnStateRestricted = "restricted";
static const char* kConnStateNone = "none";

// Connman network technology options.
static const char* kNetworkTechnology1Xrtt = "1xRTT";
static const char* kNetworkTechnologyEvdo = "EVDO";
static const char* kNetworkTechnologyGprs = "GPRS";
static const char* kNetworkTechnologyEdge = "EDGE";
static const char* kNetworkTechnologyUmts = "UMTS";
static const char* kNetworkTechnologyHspa = "HSPA";
static const char* kNetworkTechnologyHspaPlus = "HSPA+";
static const char* kNetworkTechnologyLte = "LTE";
static const char* kNetworkTechnologyLteAdvanced = "LTE Advanced";

// Connman roaming state options
static const char* kRoamingStateHome = "home";
static const char* kRoamingStateRoaming = "roaming";
static const char* kRoamingStateUnknown = "unknown";

// How long we should remember that cellular plan payment was received.
const int kRecentPlanPaymentHours = 6;

static ConnectionState ParseState(const std::string& state) {
  if (state == kStateIdle)
    return STATE_IDLE;
  if (state == kStateCarrier)
    return STATE_CARRIER;
  if (state == kStateAssociation)
    return STATE_ASSOCIATION;
  if (state == kStateConfiguration)
    return STATE_CONFIGURATION;
  if (state == kStateReady)
    return STATE_READY;
  if (state == kStateDisconnect)
    return STATE_DISCONNECT;
  if (state == kStateFailure)
    return STATE_FAILURE;
  if (state == kStateActivationFailure)
    return STATE_ACTIVATION_FAILURE;
  return STATE_UNKNOWN;
}

static ActivationState ParseActivationState(
    const std::string& activation_state) {
  if (activation_state == kActivationStateActivated)
    return ACTIVATION_STATE_ACTIVATED;
  if (activation_state == kActivationStateActivating)
    return ACTIVATION_STATE_ACTIVATING;
  if (activation_state == kActivationStateNotActivated)
    return ACTIVATION_STATE_NOT_ACTIVATED;
  if (activation_state == kActivationStateUnknown)
    return ACTIVATION_STATE_UNKNOWN;
  if (activation_state == kActivationStatePartiallyActivated)
    return ACTIVATION_STATE_PARTIALLY_ACTIVATED;
  return ACTIVATION_STATE_UNKNOWN;
}

static ConnectivityState ParseConnectivityState(const std::string& state) {
  if (state == kConnStateUnrestricted)
    return CONN_STATE_UNRESTRICTED;
  if (state == kConnStateRestricted)
    return CONN_STATE_RESTRICTED;
  if (state == kConnStateNone)
    return CONN_STATE_NONE;
  return CONN_STATE_UNKNOWN;
}

static NetworkTechnology ParseNetworkTechnology(
    const std::string& technology) {
    if (technology == kNetworkTechnology1Xrtt)
    return NETWORK_TECHNOLOGY_1XRTT;
  if (technology == kNetworkTechnologyEvdo)
    return NETWORK_TECHNOLOGY_EVDO;
  if (technology == kNetworkTechnologyGprs)
    return NETWORK_TECHNOLOGY_GPRS;
  if (technology == kNetworkTechnologyEdge)
    return NETWORK_TECHNOLOGY_EDGE;
  if (technology == kNetworkTechnologyUmts)
    return NETWORK_TECHNOLOGY_UMTS;
  if (technology == kNetworkTechnologyHspa)
    return NETWORK_TECHNOLOGY_HSPA;
  if (technology == kNetworkTechnologyHspaPlus)
    return NETWORK_TECHNOLOGY_HSPA_PLUS;
  if (technology == kNetworkTechnologyLte)
    return NETWORK_TECHNOLOGY_LTE;
  if (technology == kNetworkTechnologyLteAdvanced)
    return NETWORK_TECHNOLOGY_LTE_ADVANCED;
  return NETWORK_TECHNOLOGY_UNKNOWN;
}

static NetworkRoamingState ParseRoamingState(
    const std::string& roaming_state) {
    if (roaming_state == kRoamingStateHome)
      return ROAMING_STATE_HOME;
  if (roaming_state == kRoamingStateRoaming)
    return ROAMING_STATE_ROAMING;
  if (roaming_state == kRoamingStateUnknown)
    return ROAMING_STATE_UNKNOWN;
  return ROAMING_STATE_UNKNOWN;
}

}

// Helper function to wrap Html with <th> tag.
static std::string WrapWithTH(std::string text) {
  return "<th>" + text + "</th>";
}

// Helper function to wrap Html with <td> tag.
static std::string WrapWithTD(std::string text) {
  return "<td>" + text + "</td>";
}

// Helper function to create an Html table header for a Network.
static std::string ToHtmlTableHeader(Network* network) {
  std::string str;
  if (network->type() == TYPE_ETHERNET) {
    str += WrapWithTH("Active");
  } else if (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR) {
    str += WrapWithTH("Name") + WrapWithTH("Active") +
        WrapWithTH("Auto-Connect") + WrapWithTH("Strength");
    if (network->type() == TYPE_WIFI)
      str += WrapWithTH("Encryption") + WrapWithTH("Passphrase") +
          WrapWithTH("Identity") + WrapWithTH("Certificate");
  }
  str += WrapWithTH("State") + WrapWithTH("Error") + WrapWithTH("IP Address");
  return str;
}

// Helper function to create an Html table row for a Network.
static std::string ToHtmlTableRow(Network* network) {
  std::string str;
  if (network->type() == TYPE_ETHERNET) {
    str += WrapWithTD(base::IntToString(network->is_active()));
  } else if (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR) {
    WirelessNetwork* wireless = static_cast<WirelessNetwork*>(network);
    str += WrapWithTD(wireless->name()) +
        WrapWithTD(base::IntToString(network->is_active())) +
        WrapWithTD(base::IntToString(wireless->auto_connect())) +
        WrapWithTD(base::IntToString(wireless->strength()));
    if (network->type() == TYPE_WIFI) {
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      str += WrapWithTD(wifi->GetEncryptionString()) +
          WrapWithTD(std::string(wifi->passphrase().length(), '*')) +
          WrapWithTD(wifi->identity()) + WrapWithTD(wifi->cert_path());
    }
  }
  str += WrapWithTD(network->GetStateString()) +
      WrapWithTD(network->failed() ? network->GetErrorString() : "") +
      WrapWithTD(network->ip_address());
  return str;
}

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
static std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

static bool EnsureCrosLoaded() {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    return false;
  } else {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "chromeos_library calls made from non UI thread!";
      NOTREACHED();
    }
    return true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Network

Network::Network(const Network& network) {
  service_path_ = network.service_path_;
  device_path_ = network.device_path_;
  ip_address_ = network.ip_address_;
  type_ = network.type_;
  state_ = network.state_;
  error_ = network.error_;
  connectable_ = network.connectable_;
  is_active_ = network.is_active_;
}

void Network::Clear() {
  service_path_.clear();
  device_path_.clear();
  ip_address_.clear();
  type_ = TYPE_UNKNOWN;
  state_ = STATE_UNKNOWN;
  error_ = ERROR_UNKNOWN;
  connectable_ = true;
  is_active_ = false;
}

Network::Network(const ServiceInfo* service) {
  type_ = service->type;
  state_ = service->state;
  error_ = service->error;
  service_path_ = SafeString(service->service_path);
  device_path_ = SafeString(service->device_path);
  connectable_ = service->connectable;
  is_active_ = service->is_active;
  InitIPAddress();
}

// Used by GetHtmlInfo() which is called from the about:network handler.
std::string Network::GetStateString() const {
  switch (state_) {
    case STATE_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNKNOWN);
    case STATE_IDLE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_IDLE);
    case STATE_CARRIER:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CARRIER);
    case STATE_ASSOCIATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_ASSOCIATION);
    case STATE_CONFIGURATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CONFIGURATION);
    case STATE_READY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_READY);
    case STATE_DISCONNECT:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_DISCONNECT);
    case STATE_FAILURE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_FAILURE);
    case STATE_ACTIVATION_FAILURE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_STATE_ACTIVATION_FAILURE);
    default:
      // Usually no default, but changes to libcros may add states.
      break;
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

std::string Network::GetErrorString() const {
  switch (error_) {
    case ERROR_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
    case ERROR_OUT_OF_RANGE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
    case ERROR_PIN_MISSING:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
    case ERROR_DHCP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
    case ERROR_CONNECT_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
    case ERROR_BAD_PASSPHRASE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
    case ERROR_BAD_WEPKEY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
    case ERROR_ACTIVATION_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
    case ERROR_NEED_EVDO:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
    case ERROR_NEED_HOME_NETWORK:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
    case ERROR_OTASP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
    case ERROR_AAA_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
    default:
      // Usually no default, but changes to libcros may add errors.
      break;
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  // If connected, get ip config.
  if (EnsureCrosLoaded() && connected() && !device_path_.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path_.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; i++) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        if (strlen(ipconfig.address) > 0) {
          ip_address_ = ipconfig.address;
          break;
        }
      }
      FreeIPConfigStatus(ipconfig_status);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// WirelessNetwork
WirelessNetwork::WirelessNetwork(const WirelessNetwork& network)
    : Network(network) {
  name_ = network.name_;
  strength_ = network.strength_;
  auto_connect_ = network.auto_connect_;
  favorite_ = network.favorite_;
}

WirelessNetwork::WirelessNetwork(const ServiceInfo* service)
    : Network(service) {
  name_ = SafeString(service->name);
  strength_ = service->strength;
  auto_connect_ = service->auto_connect;
  favorite_ = service->favorite;
}

void WirelessNetwork::Clear() {
  Network::Clear();
  name_.clear();
  strength_ = 0;
  auto_connect_ = false;
  favorite_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// CellularDataPlan

string16 CellularDataPlan::GetPlanDesciption() const {
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_UNLIMITED_DATA,
          base::TimeFormatFriendlyDate(plan_start_time));
      break;
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_DATA,
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
                base::TimeFormatFriendlyDate(plan_start_time));
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
                base::TimeFormatFriendlyDate(plan_start_time));
    default:
      break;
    }
  }
  return string16();
}

string16 CellularDataPlan::GetRemainingWarning() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    if (remaining_time().InSeconds() <= chromeos::kCellularDataVeryLowSecs) {
      return GetPlanExpiration();
    }
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    if (remaining_data() <= chromeos::kCellularDataVeryLowBytes) {
      return l10n_util::GetStringFUTF16(
          IDS_NETWORK_DATA_REMAINING_MESSAGE,
          UTF8ToUTF16(base::Int64ToString(remaining_mbytes())));
    }
  }
  return string16();
}

string16 CellularDataPlan::GetDataRemainingDesciption() const {
  int64 remaining_bytes = remaining_data();
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_UNLIMITED);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
    }
    default:
      break;
  }
  return string16();
}

string16 CellularDataPlan::GetUsageInfo() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    return GetPlanExpiration();
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    return l10n_util::GetStringFUTF16(
        IDS_NETWORK_DATA_AVAILABLE_MESSAGE,
        UTF8ToUTF16(base::Int64ToString(remaining_mbytes())));
  }
  return string16();
}

base::TimeDelta CellularDataPlan::remaining_time() const {
  base::TimeDelta time = plan_end_time - base::Time::Now();
  return time.InMicroseconds() < 0 ? base::TimeDelta() : time;
}

int64 CellularDataPlan::remaining_minutes() const {
  return remaining_time().InMinutes();
}

int64 CellularDataPlan::remaining_data() const {
  int64 data = plan_data_bytes - data_bytes_used;
  return data < 0 ? 0 : data;
}

int64 CellularDataPlan::remaining_mbytes() const {
  return remaining_data() / (1024 * 1024);
}

string16 CellularDataPlan::GetPlanExpiration() const {
  return TimeFormat::TimeRemaining(remaining_time());
}

////////////////////////////////////////////////////////////////////////////////
// CellularNetwork

CellularNetwork::CellularNetwork()
    : WirelessNetwork(),
      activation_state_(ACTIVATION_STATE_UNKNOWN),
      network_technology_(NETWORK_TECHNOLOGY_UNKNOWN),
      roaming_state_(ROAMING_STATE_UNKNOWN),
      connectivity_state_(CONN_STATE_UNKNOWN),
      prl_version_(0) {
  type_ = TYPE_CELLULAR;
}

CellularNetwork::CellularNetwork(const CellularNetwork& network)
    : WirelessNetwork(network) {
  activation_state_ = network.activation_state_;
  network_technology_ = network.network_technology_;
  roaming_state_ = network.roaming_state_;
  connectivity_state_ = network.connectivity_state_;
  service_name_ = network.service_name_;
  operator_name_ = network.operator_name_;
  operator_code_ = network.operator_code_;
  payment_url_ = network.payment_url_;
  meid_ = network.meid_;
  imei_ = network.imei_;
  imsi_ = network.imsi_;
  esn_ = network.esn_;
  mdn_ = network.mdn_;
  min_ = network.min_;
  model_id_ = network.model_id_;
  manufacturer_ = network.manufacturer_;
  firmware_revision_ = network.firmware_revision_;
  hardware_revision_ = network.hardware_revision_;
  last_update_ = network.last_update_;
  prl_version_ = network.prl_version_;
  type_ = TYPE_CELLULAR;
}

CellularNetwork::CellularNetwork(const ServiceInfo* service)
    : WirelessNetwork(service) {
  service_name_ = SafeString(service->name);
  activation_state_ = service->activation_state;
  network_technology_ = service->network_technology;
  roaming_state_ = service->roaming_state;
  connectivity_state_ = service->connectivity_state;
  // Carrier Info
  if (service->carrier_info) {
    operator_name_ = SafeString(service->carrier_info->operator_name);
    operator_code_ = SafeString(service->carrier_info->operator_code);
    payment_url_ = SafeString(service->carrier_info->payment_url);
  }
  // Device Info
  if (service->device_info) {
    meid_ = SafeString(service->device_info->MEID);
    imei_ = SafeString(service->device_info->IMEI);
    imsi_ = SafeString(service->device_info->IMSI);
    esn_ = SafeString(service->device_info->ESN);
    mdn_ = SafeString(service->device_info->MDN);
    min_ = SafeString(service->device_info->MIN);
    model_id_ = SafeString(service->device_info->model_id);
    manufacturer_ = SafeString(service->device_info->manufacturer);
    firmware_revision_ = SafeString(service->device_info->firmware_revision);
    hardware_revision_ = SafeString(service->device_info->hardware_revision);
    last_update_ = SafeString(service->device_info->last_update);
    prl_version_ = service->device_info->PRL_version;
  }
  type_ = TYPE_CELLULAR;
}

CellularNetwork::~CellularNetwork() {
}

bool CellularNetwork::StartActivation() const {
  if (!EnsureCrosLoaded())
    return false;
  return ActivateCellularModem(service_path_.c_str(), NULL);
}

void CellularNetwork::Clear() {
  WirelessNetwork::Clear();
  activation_state_ = ACTIVATION_STATE_UNKNOWN;
  roaming_state_ = ROAMING_STATE_UNKNOWN;
  network_technology_ = NETWORK_TECHNOLOGY_UNKNOWN;
  connectivity_state_ = CONN_STATE_UNKNOWN;
  service_name_.clear();
  operator_name_.clear();
  operator_code_.clear();
  payment_url_.clear();
  meid_.clear();
  imei_.clear();
  imsi_.clear();
  esn_.clear();
  mdn_.clear();
  min_.clear();
  model_id_.clear();
  manufacturer_.clear();
  firmware_revision_.clear();
  hardware_revision_.clear();
  last_update_.clear();
  prl_version_ = 0;
}

const CellularDataPlan* CellularNetwork::GetSignificantDataPlan() const {
  const CellularDataPlan* significant = NULL;
  const CellularDataPlanVector& plans = GetDataPlans();
  for (CellularDataPlanVector::const_iterator iter = plans.begin();
       iter != plans.end();
       ++iter) {
    // Set significant to the first plan or to first non metered base plan.
    if (significant == NULL ||
        significant->plan_type == CELLULAR_DATA_PLAN_METERED_BASE)
      significant = *iter;
  }
  return significant;
}

CellularNetwork::DataLeft CellularNetwork::GetDataLeft() const {
  // If we need a new plan, then there's no data.
  if (needs_new_plan())
    return DATA_NONE;
  const CellularDataPlan* plan = GetSignificantDataPlan();
  if (!plan)
    return DATA_NORMAL;
  if (plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED) {
    base::TimeDelta remaining = plan->remaining_time();
    if (remaining <= base::TimeDelta::FromSeconds(0))
      return DATA_NONE;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataVeryLowSecs))
      return DATA_VERY_LOW;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataLowSecs))
      return DATA_LOW;
  } else if (plan->plan_type == CELLULAR_DATA_PLAN_METERED_PAID ||
             plan->plan_type == CELLULAR_DATA_PLAN_METERED_BASE) {
    int64 remaining = plan->remaining_data();
    if (remaining <= 0)
      return DATA_NONE;
    if (remaining <= kCellularDataVeryLowBytes)
      return DATA_VERY_LOW;
    // For base plans, we do not care about low data.
    if (remaining <= kCellularDataLowBytes &&
        plan->plan_type != CELLULAR_DATA_PLAN_METERED_BASE)
      return DATA_LOW;
  }
  return DATA_NORMAL;
}

std::string CellularNetwork::GetNetworkTechnologyString() const {
  // No need to localize these cellular technology abbreviations.
  switch (network_technology_) {
    case NETWORK_TECHNOLOGY_1XRTT:
      return "1xRTT";
      break;
    case NETWORK_TECHNOLOGY_EVDO:
      return "EVDO";
      break;
    case NETWORK_TECHNOLOGY_GPRS:
      return "GPRS";
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      return "EDGE";
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      return "UMTS";
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      return "HSPA";
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      return "HSPA Plus";
      break;
    case NETWORK_TECHNOLOGY_LTE:
      return "LTE";
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      return "LTE Advanced";
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_CELLULAR_TECHNOLOGY_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetConnectivityStateString() const {
  // These strings do not appear in the UI, so no need to localize them
  switch (connectivity_state_) {
    case CONN_STATE_UNRESTRICTED:
      return "unrestricted";
      break;
    case CONN_STATE_RESTRICTED:
      return "restricted";
      break;
    case CONN_STATE_NONE:
      return "none";
      break;
    case CONN_STATE_UNKNOWN:
    default:
      return "unknown";
  }
}

std::string CellularNetwork::ActivationStateToString(
    ActivationState activation_state) {
  switch (activation_state) {
    case ACTIVATION_STATE_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATED);
      break;
    case ACTIVATION_STATE_ACTIVATING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATING);
      break;
    case ACTIVATION_STATE_NOT_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_NOT_ACTIVATED);
      break;
    case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_PARTIALLY_ACTIVATED);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetActivationStateString() const {
  return ActivationStateToString(this->activation_state_);
}

std::string CellularNetwork::GetRoamingStateString() const {
  switch (this->roaming_state_) {
    case ROAMING_STATE_HOME:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_HOME);
      break;
    case ROAMING_STATE_ROAMING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_ROAMING);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_UNKNOWN);
      break;
  };
}


////////////////////////////////////////////////////////////////////////////////
// WifiNetwork

WifiNetwork::WifiNetwork()
    : WirelessNetwork(),
      encryption_(SECURITY_NONE) {
  type_ = TYPE_WIFI;
}

WifiNetwork::WifiNetwork(const WifiNetwork& network)
    : WirelessNetwork(network) {
  encryption_ = network.encryption_;
  passphrase_ = network.passphrase_;
  passphrase_required_ = network.passphrase_required_;
  identity_ = network.identity_;
  cert_path_ = network.cert_path_;
}

WifiNetwork::WifiNetwork(const ServiceInfo* service)
    : WirelessNetwork(service) {
  encryption_ = service->security;
  // TODO(stevenjb): Remove this if/when flimflam handles multiple profiles.
  if (UserManager::Get()->current_user_is_owner())
    passphrase_ = SafeString(service->passphrase);
  else
    passphrase_.clear();
  passphrase_required_ = service->passphrase_required;
  identity_ = SafeString(service->identity);
  cert_path_ = SafeString(service->cert_path);
  type_ = TYPE_WIFI;
}

void WifiNetwork::Clear() {
  WirelessNetwork::Clear();
  encryption_ = SECURITY_NONE;
  passphrase_.clear();
  identity_.clear();
  cert_path_.clear();
}

std::string WifiNetwork::GetEncryptionString() {
  switch (encryption_) {
    case SECURITY_UNKNOWN:
      break;
    case SECURITY_NONE:
      return "";
    case SECURITY_WEP:
      return "WEP";
    case SECURITY_WPA:
      return "WPA";
    case SECURITY_RSN:
      return "RSN";
    case SECURITY_8021X:
      return "8021X";
  }
  return "Unknown";
}

bool WifiNetwork::IsPassphraseRequired() const {
  // TODO(stevenjb): Remove error_ tests when fixed in flimflam
  // (http://crosbug.com/10135).
  if (error_ == ERROR_BAD_PASSPHRASE || error_ == ERROR_BAD_WEPKEY)
    return true;
  return passphrase_required_;
}

// Parse 'path' to determine if the certificate is stored in a pkcs#11 device.
// flimflam recognizes the string "SETTINGS:" to specify authentication
// parameters. 'key_id=' indicates that the certificate is stored in a pkcs#11
// device. See src/third_party/flimflam/files/doc/service-api.txt.
bool WifiNetwork::IsCertificateLoaded() const {
  static const std::string settings_string("SETTINGS:");
  static const std::string pkcs11_key("key_id");
  if (cert_path_.find(settings_string) == 0) {
    std::string::size_type idx = cert_path_.find(pkcs11_key);
    if (idx != std::string::npos)
      idx = cert_path_.find_first_not_of(kWhitespaceASCII,
                                         idx + pkcs11_key.length());
    if (idx != std::string::npos && cert_path_[idx] == '=')
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

class NetworkLibraryImpl : public NetworkLibrary  {
 public:
  NetworkLibraryImpl()
      : network_manager_monitor_(NULL),
        data_plan_monitor_(NULL),
        ethernet_(NULL),
        wifi_(NULL),
        cellular_(NULL),
        available_devices_(0),
        enabled_devices_(0),
        connected_devices_(0),
        wifi_scanning_(false),
        offline_mode_(false),
        is_locked_(false),
        update_task_(NULL) {
    if (EnsureCrosLoaded()) {
      Init();
      network_manager_monitor_ =
          MonitorNetworkManager(&NetworkManagerStatusChangedHandler,
                                this);
      data_plan_monitor_ = MonitorCellularDataPlan(&DataPlanUpdateHandler,
                                                   this);
      network_login_observer_.reset(new NetworkLoginObserver(this));
    } else {
      InitTestData();
    }
  }

  ~NetworkLibraryImpl() {
    network_manager_observers_.Clear();
    if (network_manager_monitor_)
      DisconnectPropertyChangeMonitor(network_manager_monitor_);
    data_plan_observers_.Clear();
    if (data_plan_monitor_)
      DisconnectDataPlanUpdateMonitor(data_plan_monitor_);
    STLDeleteValues(&network_observers_);
    ClearNetworks();
  }

  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {
    if (!network_manager_observers_.HasObserver(observer))
      network_manager_observers_.AddObserver(observer);
  }

  void NetworkStatusChanged() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (update_task_) {
      update_task_->Cancel();
    }
    update_task_ =
        NewRunnableMethod(this,
                          &NetworkLibraryImpl::UpdateNetworkManagerStatus);
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE, update_task_,
        kNetworkUpdateDelayMs);
  }

  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {
    network_manager_observers_.RemoveObserver(observer);
  }

  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {
    DCHECK(observer);
    if (!EnsureCrosLoaded())
      return;
    // First, add the observer to the callback map.
    NetworkObserverMap::iterator iter = network_observers_.find(service_path);
    NetworkObserverList* oblist;
    if (iter != network_observers_.end()) {
      oblist = iter->second;
    } else {
      std::pair<NetworkObserverMap::iterator, bool> inserted =
        network_observers_.insert(
            std::make_pair<std::string, NetworkObserverList*>(
                service_path,
                new NetworkObserverList(this, service_path)));
      oblist = inserted.first->second;
    }
    if (!oblist->HasObserver(observer))
      oblist->AddObserver(observer);
  }

  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {
    DCHECK(observer);
    DCHECK(service_path.size());
    NetworkObserverMap::iterator map_iter =
        network_observers_.find(service_path);
    if (map_iter != network_observers_.end()) {
      map_iter->second->RemoveObserver(observer);
      if (!map_iter->second->size()) {
        delete map_iter->second;
        network_observers_.erase(map_iter++);
      }
    }
  }

  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {
    DCHECK(observer);
    NetworkObserverMap::iterator map_iter = network_observers_.begin();
    while (map_iter != network_observers_.end()) {
      map_iter->second->RemoveObserver(observer);
      if (!map_iter->second->size()) {
        delete map_iter->second;
        network_observers_.erase(map_iter++);
      } else {
        ++map_iter;
      }
    }
  }

  virtual void Lock() {
    if (is_locked_)
      return;
    is_locked_ = true;
    NotifyNetworkManagerChanged();
 }

  virtual void Unlock() {
    DCHECK(is_locked_);
    if (!is_locked_)
      return;
    is_locked_ = false;
    NotifyNetworkManagerChanged();
  }

  virtual bool IsLocked() {
    return is_locked_;
  }

  virtual void AddCellularDataPlanObserver(CellularDataPlanObserver* observer) {
    if (!data_plan_observers_.HasObserver(observer))
      data_plan_observers_.AddObserver(observer);
  }

  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {
    data_plan_observers_.RemoveObserver(observer);
  }

  virtual const EthernetNetwork* ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const {
    return ethernet_ ? ethernet_->connecting() : false;
  }
  virtual bool ethernet_connected() const {
    return ethernet_ ? ethernet_->connected() : false;
  }

  virtual const WifiNetwork* wifi_network() const { return wifi_; }
  virtual bool wifi_connecting() const {
    return wifi_ ? wifi_->connecting() : false;
  }
  virtual bool wifi_connected() const {
    return wifi_ ? wifi_->connected() : false;
  }

  virtual const CellularNetwork* cellular_network() const { return cellular_; }
  virtual bool cellular_connecting() const {
    return cellular_ ? cellular_->connecting() : false;
  }
  virtual bool cellular_connected() const {
    return cellular_ ? cellular_->connected() : false;
  }

  bool Connected() const {
    return ethernet_connected() || wifi_connected() || cellular_connected();
  }

  bool Connecting() const {
    return ethernet_connecting() || wifi_connecting() || cellular_connecting();
  }

  const std::string& IPAddress() const {
    // Returns IP address for the active network.
    const Network* active = active_network();
    if (active != NULL)
      return active->ip_address();
    if (ethernet_)
      return ethernet_->ip_address();
    static std::string null_address("0.0.0.0");
    return null_address;
  }

  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return remembered_wifi_networks_;
  }

  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  /////////////////////////////////////////////////////////////////////////////

  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) {
    return GetWirelessNetworkByPath(wifi_networks_, path);
  }

  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) {
    return GetWirelessNetworkByPath(cellular_networks_, path);
  }

  virtual void RequestWifiScan() {
    if (EnsureCrosLoaded() && wifi_enabled()) {
      wifi_scanning_ = true;  // Cleared in UpdateNetworkManagerStatus.
      RequestScan(TYPE_WIFI);
    }
  }

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    if (!EnsureCrosLoaded())
      return false;
    DeviceNetworkList* network_list = GetDeviceNetworkList();
    if (network_list == NULL)
      return false;
    result->clear();
    result->reserve(network_list->network_size);
    const base::Time now = base::Time::Now();
    for (size_t i = 0; i < network_list->network_size; ++i) {
      DCHECK(network_list->networks[i].address);
      DCHECK(network_list->networks[i].name);
      WifiAccessPoint ap;
      ap.mac_address = SafeString(network_list->networks[i].address);
      ap.name = SafeString(network_list->networks[i].name);
      ap.timestamp = now -
          base::TimeDelta::FromSeconds(network_list->networks[i].age_seconds);
      ap.signal_strength = network_list->networks[i].strength;
      ap.channel = network_list->networks[i].channel;
      result->push_back(ap);
    }
    FreeDeviceNetworkList(network_list);
    return true;
  }

  virtual bool ConnectToWifiNetwork(WifiNetwork* network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    DCHECK(network);
    if (!EnsureCrosLoaded())
      return true;  // No library loaded, don't trigger a retry attempt.
    // TODO(ers) make wifi the highest priority service type
    if (ConnectToNetworkWithCertInfo(network->service_path().c_str(),
        password.empty() ? NULL : password.c_str(),
        identity.empty() ? NULL : identity.c_str(),
        certpath.empty() ? NULL : certpath.c_str())) {
      // Update local cache and notify listeners.
      WifiNetwork* wifi = GetWirelessNetworkByPath(
          wifi_networks_, network->service_path());
      if (wifi) {
        wifi->set_passphrase(password);
        wifi->set_identity(identity);
        wifi->set_cert_path(certpath);
        wifi->set_connecting(true);
        wifi_ = wifi;
      }
      NotifyNetworkManagerChanged();
      return true;
    } else {
      // The only likely cause for an immediate failure is a badly formatted
      // passphrase. TODO(stevenjb): get error information from libcros
      // and call set_error correctly. crosbug.com/9538.
      network->set_error(ERROR_BAD_PASSPHRASE);
      return false;  // Immediate failure.
    }
  }

  virtual bool ConnectToWifiNetwork(ConnectionSecurity security,
                                    const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) {
    if (!EnsureCrosLoaded())
      return true;  // No library loaded, don't trigger a retry attempt.
    // First create a service from hidden network.
    ServiceInfo* service = GetWifiService(ssid.c_str(), security);
    if (service) {
      // Set auto-connect.
      SetAutoConnect(service->service_path, auto_connect);
      // Now connect to that service.
      // TODO(ers) make wifi the highest priority service type
      bool res = ConnectToNetworkWithCertInfo(
          service->service_path,
          password.empty() ? NULL : password.c_str(),
          identity.empty() ? NULL : identity.c_str(),
          certpath.empty() ? NULL : certpath.c_str());

      // Clean up ServiceInfo object.
      FreeServiceInfo(service);
      return res;
    } else {
      LOG(WARNING) << "Cannot find hidden network: " << ssid;
      // TODO(chocobo): Show error message.
      return false;  // Immediate failure.
    }
  }

  virtual bool ConnectToCellularNetwork(const CellularNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded())
      return true;  // No library loaded, don't trigger a retry attempt.
    // TODO(ers) make cellular the highest priority service type
    if (network && ConnectToNetwork(network->service_path().c_str(), NULL)) {
      // Update local cache and notify listeners.
      CellularNetwork* cellular = GetWirelessNetworkByPath(
          cellular_networks_, network->service_path());
      if (cellular) {
        cellular->set_connecting(true);
        cellular_ = cellular;
      }
      NotifyNetworkManagerChanged();
      return true;
    } else {
      return false;  // Immediate failure.
    }
  }

  virtual void RefreshCellularDataPlans(const CellularNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    RequestCellularDataPlanUpdate(network->service_path().c_str());
  }

  // Records information that cellular play payment had happened.
  virtual void SignalCellularPlanPayment() {
    DCHECK(!HasRecentCellularPlanPayment());
    cellular_plan_payment_time_ = base::Time::Now();
  }

  // Returns true if cellular plan payment had been recorded recently.
  virtual bool HasRecentCellularPlanPayment() {
    return (base::Time::Now() -
              cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
  }

  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    // TODO(ers) restore default service type priority ordering?
    if (DisconnectFromNetwork(network->service_path().c_str())) {
      // Update local cache and notify listeners.
      if (network->type() == TYPE_WIFI) {
        WifiNetwork* wifi = GetWirelessNetworkByPath(
            wifi_networks_, network->service_path());
        if (wifi) {
          wifi->set_connected(false);
          wifi_ = NULL;
        }
      } else if (network->type() == TYPE_CELLULAR) {
        CellularNetwork* cellular = GetWirelessNetworkByPath(
            cellular_networks_, network->service_path());
        if (cellular) {
          cellular->set_connected(false);
          cellular_ = NULL;
        }
      }
      NotifyNetworkManagerChanged();
    }
  }

  virtual void SaveCellularNetwork(const CellularNetwork* network) {
    DCHECK(network);
    // Update the cellular network with libcros.
    if (!EnsureCrosLoaded() || !network)
      return;
    CellularNetwork* cellular =
        GetWirelessNetworkByPath(cellular_networks_,
                                 network->service_path());
    if (!cellular) {
      LOG(WARNING) << "Save to unknown network: " << cellular->service_path();
      return;
    }

    // Immediately update properties in the cached structure.
    cellular->set_auto_connect(network->auto_connect());
    // Update libcros.
    SetAutoConnect(network->service_path().c_str(), network->auto_connect());
  }

  virtual void SaveWifiNetwork(const WifiNetwork* network) {
    DCHECK(network);
    // Update the wifi network with libcros.
    if (!EnsureCrosLoaded() || !network)
      return;
    WifiNetwork* wifi = GetWirelessNetworkByPath(wifi_networks_,
                                                 network->service_path());
    if (!wifi) {
      LOG(WARNING) << "Save to unknown network: " << wifi->service_path();
      return;
    }
    // Immediately update properties in the cached structure.
    wifi->set_passphrase(network->passphrase());
    wifi->set_identity(network->identity());
    wifi->set_cert_path(network->cert_path());
    wifi->set_auto_connect(network->auto_connect());
    // Update libcros.
    const char* service_path = network->service_path().c_str();
    SetPassphrase(service_path, network->passphrase().c_str());
    SetIdentity(service_path, network->identity().c_str());
    SetCertPath(service_path, network->cert_path().c_str());
    SetAutoConnect(service_path, network->auto_connect());
  }

  virtual void ForgetWifiNetwork(const std::string& service_path) {
    if (!EnsureCrosLoaded())
      return;
    // NOTE: service paths for remembered wifi networks do not match the
    // service paths in wifi_networks_; calling a libcros funtion that
    // operates on the wifi_networks_ list with this service_path will
    // trigger a crash because the DBUS path does not exist.
    // TODO(stevenjb): modify libcros to warn and fail instead of crash.
    // https://crosbug.com/9295
    if (DeleteRememberedService(service_path.c_str())) {
      // Update local cache and notify listeners.
      for (WifiNetworkVector::iterator iter =
               remembered_wifi_networks_.begin();
          iter != remembered_wifi_networks_.end();
          ++iter) {
        if ((*iter)->service_path() == service_path) {
          delete (*iter);
          remembered_wifi_networks_.erase(iter);
          break;
        }
      }
      NotifyNetworkManagerChanged();
    }
  }

  virtual bool ethernet_available() const {
    return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const {
    return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const {
    return available_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool ethernet_enabled() const {
    return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const {
    return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const {
    return enabled_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool wifi_scanning() const {
    return wifi_scanning_;
  }

  virtual bool offline_mode() const { return offline_mode_; }

  virtual const Network* active_network() const {
    if (ethernet_ && ethernet_->is_active())
      return ethernet_;
    if (wifi_ && wifi_->is_active())
      return wifi_;
    if (cellular_ && cellular_->is_active())
      return cellular_;
    // Due to bug chromium-os:9310, if no active network is found,
    // use the first connected.
    // TODO(chocobo): Remove when bug 9310 is fixed.
    // START BUG 9310 WORKAROUND
    if (ethernet_ && ethernet_->connected()) {
      ethernet_->set_active(true);
      return ethernet_;
    }
    if (wifi_ && wifi_->connected()) {
      wifi_->set_active(true);
      return wifi_;
    }
    if (cellular_ && cellular_->connected()) {
      cellular_->set_active(true);
      return cellular_;
    }
    // END BUG 9310 WORKAROUND
    return NULL;
  }

  virtual void EnableEthernetNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_ETHERNET, enable);
  }

  virtual void EnableWifiNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_WIFI, enable);
  }

  virtual void EnableCellularNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_CELLULAR, enable);
  }

  virtual void EnableOfflineMode(bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && offline_mode_) {
      VLOG(1) << "Trying to enable offline mode when it's already enabled.";
      return;
    }
    if (!enable && !offline_mode_) {
      VLOG(1) << "Trying to disable offline mode when it's already disabled.";
      return;
    }

    if (SetOfflineMode(enable)) {
      offline_mode_ = enable;
    }
  }

  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address) {
    hardware_address->clear();
    NetworkIPConfigVector ipconfig_vector;
    if (EnsureCrosLoaded() && !device_path.empty()) {
      IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
      if (ipconfig_status) {
        for (int i = 0; i < ipconfig_status->size; i++) {
          IPConfig ipconfig = ipconfig_status->ips[i];
          ipconfig_vector.push_back(
              NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                              ipconfig.netmask, ipconfig.gateway,
                              ipconfig.name_servers));
        }
        *hardware_address = ipconfig_status->hardware_address;
        FreeIPConfigStatus(ipconfig_status);
        // Sort the list of ip configs by type.
        std::sort(ipconfig_vector.begin(), ipconfig_vector.end());
      }
    }
    return ipconfig_vector;
  }

  virtual std::string GetHtmlInfo(int refresh) {
    std::string output;
    output.append("<html><head><title>About Network</title>");
    if (refresh > 0)
      output.append("<meta http-equiv=\"refresh\" content=\"" +
          base::IntToString(refresh) + "\"/>");
    output.append("</head><body>");
    if (refresh > 0) {
      output.append("(Auto-refreshing page every " +
                    base::IntToString(refresh) + "s)");
    } else {
      output.append("(To auto-refresh this page: about:network/&lt;secs&gt;)");
    }

    if (ethernet_enabled()) {
      output.append("<h3>Ethernet:</h3><table border=1>");
      if (ethernet_) {
        output.append("<tr>" + ToHtmlTableHeader(ethernet_) + "</tr>");
        output.append("<tr>" + ToHtmlTableRow(ethernet_) + "</tr>");
      }
    }

    if (wifi_enabled()) {
      output.append("</table><h3>Wifi:</h3><table border=1>");
      for (size_t i = 0; i < wifi_networks_.size(); ++i) {
        if (i == 0)
          output.append("<tr>" + ToHtmlTableHeader(wifi_networks_[i]) +
                        "</tr>");
        output.append("<tr>" + ToHtmlTableRow(wifi_networks_[i]) + "</tr>");
      }
    }

    if (cellular_enabled()) {
      output.append("</table><h3>Cellular:</h3><table border=1>");
      for (size_t i = 0; i < cellular_networks_.size(); ++i) {
        if (i == 0)
          output.append("<tr>" + ToHtmlTableHeader(cellular_networks_[i]) +
                        "</tr>");
        output.append("<tr>" + ToHtmlTableRow(cellular_networks_[i]) + "</tr>");
      }
    }

    output.append("</table><h3>Remembered Wifi:</h3><table border=1>");
    for (size_t i = 0; i < remembered_wifi_networks_.size(); ++i) {
      if (i == 0)
        output.append(
            "<tr>" + ToHtmlTableHeader(remembered_wifi_networks_[i]) +
            "</tr>");
      output.append("<tr>" + ToHtmlTableRow(remembered_wifi_networks_[i]) +
          "</tr>");
    }

    output.append("</table></body></html>");
    return output;
  }

 private:

  class NetworkObserverList : public ObserverList<NetworkObserver> {
   public:
    NetworkObserverList(NetworkLibraryImpl* library,
                        const std::string& service_path) {
      network_monitor_ = MonitorNetworkService(&NetworkStatusChangedHandler,
                                               service_path.c_str(),
                                               library);
    }

    virtual ~NetworkObserverList() {
      if (network_monitor_)
        DisconnectPropertyChangeMonitor(network_monitor_);
    }

   private:
    static void NetworkStatusChangedHandler(void* object,
                                            const char* path,
                                            const char* key,
                                            const Value* value) {
      NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
      DCHECK(networklib);
      networklib->UpdateNetworkStatus(path, key, value);
    }
    PropertyChangeMonitor network_monitor_;
  };

  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  static void NetworkManagerStatusChangedHandler(void* object,
                                                 const char* path,
                                                 const char* key,
                                                 const Value* value) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    networklib->NetworkStatusChanged();
  }

  static void DataPlanUpdateHandler(void* object,
                                    const char* modem_service_path,
                                    const CellularDataPlanList* dataplan) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    if (!networklib || !networklib->cellular_network()) {
      // This might happen if an update is received as we are shutting down.
      return;
    }
    // Store data plan for currently connected cellular network.
    if (networklib->cellular_network()->service_path()
        .compare(modem_service_path) == 0) {
      if (dataplan != NULL) {
        networklib->UpdateCellularDataPlan(dataplan);
      }
    }
  }

  void ParseSystem(SystemInfo* system) {
    std::string prev_cellular_service_path = cellular_ ?
        cellular_->service_path() : std::string();
    bool prev_cellular_connected = cellular_ ?
        cellular_->connected() : false;
    std::vector<CellularDataPlan> prev_cellular_data_plans;
    if (cellular_) {
      const CellularDataPlanVector& plans = cellular_->GetDataPlans();
      for (CellularDataPlanVector::const_iterator iter = plans.begin();
           iter != plans.end();
           ++iter) {
        prev_cellular_data_plans.push_back(**iter);
      }
    }

    ClearNetworks();
    available_devices_ = system->available_technologies;
    enabled_devices_ = system->enabled_technologies;
    connected_devices_ = system->connected_technologies;
    offline_mode_ = system->offline_mode;

    DVLOG(1) << "ParseSystem:";
    for (int i = 0; i < system->service_size; i++) {
      const ServiceInfo* service = system->GetServiceInfo(i);
      DVLOG(1) << "  (" << service->type << ") " << service->name
               << " mode=" << service->mode
               << " state=" << service->state
               << " sec=" << service->security
               << " req=" << service->passphrase_required
               << " pass=" << service->passphrase
               << " id=" << service->identity
               << " certpath=" << service->cert_path
               << " str=" << service->strength
               << " fav=" << service->favorite
               << " auto=" << service->auto_connect
               << " is_active=" << service->is_active
               << " error=" << service->error;
      // Once a connected ethernet service is found, disregard other ethernet
      // services that are also found
      if (service->type == TYPE_ETHERNET) {
        // There could be multiple ethernet services (eth0, dummy0, etc)
        // In this case, once you find a connected service, ignore the
        // other ones.  Otherwise, you may choose an ethernet service
        // that is not connected.
        if (ethernet_enabled() &&
            (ethernet_ == NULL || !(ethernet_->connected()))) {
          // If previous ethernet was previously created, free it first
          if (ethernet_ != NULL)
            delete ethernet_;
          ethernet_ = new EthernetNetwork(service);
        }
      } else if (service->type == TYPE_WIFI) {
        // Sometimes flimflam still returns wifi networks when disabled.
        // We don't want to show these in the UI.
        if (wifi_enabled())
          wifi_networks_.push_back(new WifiNetwork(service));
      } else if (service->type == TYPE_CELLULAR) {
        // Sometimes flimflam still returns cellular networks when disabled.
        // We don't want to show these in the UI.
        if (cellular_enabled())
          cellular_networks_.push_back(new CellularNetwork(service));
      }
    }

    // Create placeholder network for ethernet even if the service is not
    // detected at this moment.
    if (!ethernet_)
      ethernet_ = new EthernetNetwork();

    DVLOG(1) << "Remembered networks:";
    for (int i = 0; i < system->remembered_service_size; i++) {
      const ServiceInfo* service = system->GetRememberedServiceInfo(i);
      // All services in the remembered list are "favorites" even though
      // they do not explicitly set the "favorite" property.
      DVLOG(1) << "  (" << service->type << ") " << service->name
               << " mode=" << service->mode
               << " sec=" << service->security
               << " pass=" << service->passphrase
               << " id=" << service->identity
               << " certpath=" << service->cert_path
               << " auto=" << service->auto_connect;
      if (service->type == TYPE_WIFI) {
        remembered_wifi_networks_.push_back(new WifiNetwork(service));
      }
    }

    // Find the active wifi network (if any).
    wifi_ = NULL;
    for (size_t i = 0; i < wifi_networks_.size(); i++) {
      if (wifi_networks_[i]->connecting_or_connected()) {
        wifi_ = wifi_networks_[i];
        break;  // There is only one connected or connecting wifi network.
      }
    }

    // Find the active cellular network (if any).
    cellular_ = NULL;
    for (size_t i = 0; i < cellular_networks_.size(); i++) {
      if (cellular_networks_[i]->connecting_or_connected()) {
        cellular_ = cellular_networks_[i];
        // If refreshing previous cellular, then copy over prev data plans.
        if (cellular_->service_path() == prev_cellular_service_path) {
          for (std::vector<CellularDataPlan>::iterator iter =
                   prev_cellular_data_plans.begin();
               iter != prev_cellular_data_plans.end();
               ++iter) {
            cellular_->data_plans_.push_back(new CellularDataPlan(*iter));
          }
        } else if (!prev_cellular_connected && cellular_->connected()) {
          // If new cellular, then request update of the data plan list.
          RefreshCellularDataPlans(cellular_);
        }
        break;  // There is only one connected or connecting cellular network.
      }
    }

    wifi_scanning_ = false;
    for (int i = 0; i < system->device_size; i++) {
      const DeviceInfo* device = system->GetDeviceInfo(i);
      if (device->type == TYPE_WIFI && device->scanning)
        wifi_scanning_ = true;
    }
  }

  void Init() {
    // First, get the currently available networks. This data is cached
    // on the connman side, so the call should be quick.
    VLOG(1) << "Getting initial CrOS network info.";
    UpdateSystemInfo();
  }

  void InitTestData() {
    is_locked_ = true;
    ethernet_ = new EthernetNetwork();
    ethernet_->set_connected(true);
    ethernet_->set_service_path("eth1");

    STLDeleteElements(&wifi_networks_);
    wifi_networks_.clear();
    WifiNetwork* wifi1 = new WifiNetwork();
    wifi1->set_service_path("fw1");
    wifi1->set_name("Fake Wifi 1");
    wifi1->set_strength(90);
    wifi1->set_connected(false);
    wifi1->set_encryption(SECURITY_NONE);
    wifi_networks_.push_back(wifi1);

    WifiNetwork* wifi2 = new WifiNetwork();
    wifi2->set_service_path("fw2");
    wifi2->set_name("Fake Wifi 2");
    wifi2->set_strength(70);
    wifi2->set_connected(true);
    wifi2->set_encryption(SECURITY_WEP);
    wifi_networks_.push_back(wifi2);

    WifiNetwork* wifi3 = new WifiNetwork();
    wifi3->set_service_path("fw3");
    wifi3->set_name("Fake Wifi 3");
    wifi3->set_strength(50);
    wifi3->set_connected(false);
    wifi3->set_encryption(SECURITY_8021X);
    wifi3->set_identity("nobody@google.com");
    wifi3->set_cert_path("SETTINGS:key_id=3,cert_id=3,pin=111111");
    wifi_networks_.push_back(wifi3);

    wifi_ = wifi2;

    STLDeleteElements(&cellular_networks_);
    cellular_networks_.clear();

    CellularNetwork* cellular1 = new CellularNetwork();
    cellular1->set_service_path("fc1");
    cellular1->set_name("Fake Cellular 1");
    cellular1->set_strength(70);
    cellular1->set_connected(false);
    cellular1->set_activation_state(ACTIVATION_STATE_ACTIVATED);
    cellular1->set_payment_url(std::string("http://www.google.com"));
    cellular1->set_network_technology(NETWORK_TECHNOLOGY_EVDO);

    CellularDataPlan* base_plan = new CellularDataPlan();
    base_plan->plan_name = "Base plan";
    base_plan->plan_type = CELLULAR_DATA_PLAN_METERED_BASE;
    base_plan->plan_data_bytes = 100ll * 1024 * 1024;
    base_plan->data_bytes_used = 75ll * 1024 * 1024;
    cellular1->data_plans_.push_back(base_plan);

    CellularDataPlan* paid_plan = new CellularDataPlan();
    paid_plan->plan_name = "Paid plan";
    paid_plan->plan_type = CELLULAR_DATA_PLAN_METERED_PAID;
    paid_plan->plan_data_bytes = 5ll * 1024 * 1024 * 1024;
    paid_plan->data_bytes_used = 3ll * 1024 * 1024 * 1024;
    cellular1->data_plans_.push_back(paid_plan);

    cellular_networks_.push_back(cellular1);
    cellular_ = cellular1;

    remembered_wifi_networks_.clear();
    remembered_wifi_networks_.push_back(new WifiNetwork(*wifi2));

    int devices = (1 << TYPE_ETHERNET) | (1 << TYPE_WIFI) |
        (1 << TYPE_CELLULAR);
    available_devices_ = devices;
    enabled_devices_ = devices;
    connected_devices_ = devices;
    wifi_scanning_ = false;
    offline_mode_ = false;
  }

  void UpdateSystemInfo() {
    if (EnsureCrosLoaded()) {
      UpdateNetworkManagerStatus();
    }
  }

  WifiNetwork* GetWifiNetworkByName(const std::string& name) {
    for (size_t i = 0; i < wifi_networks_.size(); ++i) {
      if (wifi_networks_[i]->name().compare(name) == 0) {
        return wifi_networks_[i];
      }
    }
    return NULL;
  }

  template<typename T> T GetWirelessNetworkByPath(
      std::vector<T>& networks, const std::string& path) {
    typedef typename std::vector<T>::iterator iter_t;
    iter_t iter = std::find_if(networks.begin(), networks.end(),
                               WirelessNetwork::ServicePathEq(path));
    return (iter != networks.end()) ? *iter : NULL;
  }

  // const version
  template<typename T> const T GetWirelessNetworkByPath(
      const std::vector<T>& networks, const std::string& path) const {
    typedef typename std::vector<T>::const_iterator iter_t;
    iter_t iter = std::find_if(networks.begin(), networks.end(),
                               WirelessNetwork::ServicePathEq(path));
    return (iter != networks.end()) ? *iter : NULL;
  }

  void EnableNetworkDeviceType(ConnectionType device, bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && (enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to enable a device that's already enabled: "
                   << device;
      return;
    }
    if (!enable && !(enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to disable a device that's already disabled: "
                   << device;
      return;
    }

    EnableNetworkDevice(device, enable);
  }

  void NotifyNetworkManagerChanged() {
    FOR_EACH_OBSERVER(NetworkManagerObserver,
                      network_manager_observers_,
                      OnNetworkManagerChanged(this));
  }

  void NotifyNetworkChanged(Network* network) {
    DCHECK(network);
    NetworkObserverMap::const_iterator iter = network_observers_.find(
        network->service_path());
    if (iter != network_observers_.end()) {
      FOR_EACH_OBSERVER(NetworkObserver,
                        *(iter->second),
                        OnNetworkChanged(this, network));
    } else {
      NOTREACHED() <<
          "There weren't supposed to be any property change observers of " <<
           network->service_path();
    }
  }

  void NotifyCellularDataPlanChanged() {
    FOR_EACH_OBSERVER(CellularDataPlanObserver,
                      data_plan_observers_,
                      OnCellularDataPlanChanged(this));
  }

  void UpdateNetworkManagerStatus() {
    // Make sure we run on UI thread.
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    update_task_ = NULL;
    VLOG(1) << "Updating Network Status";

    SystemInfo* system = GetSystemInfo();
    if (!system)
      return;

    ParseSystem(system);

    NotifyNetworkManagerChanged();
    FreeSystemInfo(system);
  }

  void UpdateNetworkStatus(const char* path,
                           const char* key,
                           const Value* value) {
    if (key == NULL || value == NULL)
      return;
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &NetworkLibraryImpl::UpdateNetworkStatus,
                            path, key, value));
      return;
    }

    bool boolval = false;
    int intval = 0;
    std::string stringval;
    Network* network = NULL;
    if (ethernet_->service_path() == path) {
      network = ethernet_;
    } else {
      CellularNetwork* cellular =
          GetWirelessNetworkByPath(cellular_networks_, path);
      WifiNetwork* wifi =
          GetWirelessNetworkByPath(wifi_networks_, path);
      if (cellular == NULL && wifi == NULL)
        return;

      WirelessNetwork* wireless = NULL;
      if (wifi != NULL)
        wireless = static_cast<WirelessNetwork*>(wifi);
      else
        wireless = static_cast<WirelessNetwork*>(cellular);

      if (strcmp(key, kSignalStrengthProperty) == 0) {
        if (value->GetAsInteger(&intval))
          wireless->set_strength(intval);
      } else if (cellular != NULL) {
        if (strcmp(key, kConnectivityStateProperty) == 0) {
          if (value->GetAsString(&stringval))
            cellular->set_connectivity_state(ParseConnectivityState(stringval));
        } else if (strcmp(key, kActivationStateProperty) == 0) {
          if (value->GetAsString(&stringval))
            cellular->set_activation_state(ParseActivationState(stringval));
        } else if (strcmp(key, kPaymentURLProperty) == 0) {
          if (value->GetAsString(&stringval))
            cellular->set_payment_url(stringval);
        } else if (strcmp(key, kNetworkTechnologyProperty) == 0) {
          if (value->GetAsString(&stringval))
            cellular->set_network_technology(
                ParseNetworkTechnology(stringval));
        } else if (strcmp(key, kRoamingStateProperty) == 0) {
          if (value->GetAsString(&stringval))
            cellular->set_roaming_state(ParseRoamingState(stringval));
        }
      }
      network = wireless;
    }
    if (strcmp(key, kConnectableProperty) == 0) {
      if (value->GetAsBoolean(&boolval))
        network->set_connectable(boolval);
    } else if (strcmp(key, kIsActiveProperty) == 0) {
      if (value->GetAsBoolean(&boolval))
        network->set_active(boolval);
    } else if (strcmp(key, kStateProperty) == 0) {
      if (value->GetAsString(&stringval)) {
        network->set_state(ParseState(stringval));
        // State changed, so refresh IP address.
        network->InitIPAddress();
      }
    }
    if (network)
      NotifyNetworkChanged(network);
  }

  void UpdateCellularDataPlan(const CellularDataPlanList* data_plans) {
    DCHECK(cellular_);
    cellular_->SetDataPlans(data_plans);
    NotifyCellularDataPlanChanged();
  }

  void ClearNetworks() {
    if (ethernet_)
      delete ethernet_;
    ethernet_ = NULL;
    wifi_ = NULL;
    cellular_ = NULL;
    STLDeleteElements(&wifi_networks_);
    wifi_networks_.clear();
    STLDeleteElements(&cellular_networks_);
    cellular_networks_.clear();
    STLDeleteElements(&remembered_wifi_networks_);
    remembered_wifi_networks_.clear();
  }

  // Network manager observer list
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // Network observer map
  NetworkObserverMap network_observers_;

  // For monitoring network manager status changes.
  PropertyChangeMonitor network_manager_monitor_;

  // For monitoring data plan changes to the connected cellular network.
  DataPlanUpdateMonitor data_plan_monitor_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

  // The ethernet network.
  EthernetNetwork* ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork* wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork* cellular_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  // True if we are currently scanning for wifi networks.
  bool wifi_scanning_;

  // Currently not implemented. TODO: implement or eliminate.
  bool offline_mode_;

  // True if access network library is locked.
  bool is_locked_;

  // Delayed task to retrieve the network information.
  CancelableTask* update_task_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImpl);
};

class NetworkLibraryStubImpl : public NetworkLibrary {
 public:
  NetworkLibraryStubImpl()
      : ip_address_("1.1.1.1"),
        ethernet_(new EthernetNetwork()),
        wifi_(NULL),
        cellular_(NULL) {
  }
  ~NetworkLibraryStubImpl() { if (ethernet_) delete ethernet_; }
  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {}
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {}
  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {}
  virtual void Lock() {}
  virtual void Unlock() {}
  virtual bool IsLocked() { return true; }
  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual const EthernetNetwork* ethernet_network() const {
    return ethernet_;
  }
  virtual bool ethernet_connecting() const { return false; }
  virtual bool ethernet_connected() const { return true; }
  virtual const WifiNetwork* wifi_network() const {
    return wifi_;
  }
  virtual bool wifi_connecting() const { return false; }
  virtual bool wifi_connected() const { return false; }
  virtual const CellularNetwork* cellular_network() const {
    return cellular_;
  }
  virtual bool cellular_connecting() const { return false; }
  virtual bool cellular_connected() const { return false; }

  bool Connected() const { return true; }
  bool Connecting() const { return false; }
  const std::string& IPAddress() const { return ip_address_; }
  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }
  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return wifi_networks_;
  }
  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }
  virtual bool has_cellular_networks() const {
    return cellular_networks_.begin() != cellular_networks_.end();
  }
  /////////////////////////////////////////////////////////////////////////////

  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) { return NULL; }
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) { return NULL; }
  virtual void RequestWifiScan() {}
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    return false;
  }

  virtual bool ConnectToWifiNetwork(WifiNetwork* network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    return true;
  }
  virtual bool ConnectToWifiNetwork(ConnectionSecurity security,
                                    const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) {
    return true;
  }
  virtual bool ConnectToCellularNetwork(const CellularNetwork* network) {
    return true;
  }
  virtual void RefreshCellularDataPlans(const CellularNetwork* network) {}
  virtual void SignalCellularPlanPayment() {}
  virtual bool HasRecentCellularPlanPayment() { return false; }
  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork* network) {}
  virtual void SaveCellularNetwork(const CellularNetwork* network) {}
  virtual void SaveWifiNetwork(const WifiNetwork* network) {}
  virtual void ForgetWifiNetwork(const std::string& service_path) {}
  virtual bool ethernet_available() const { return true; }
  virtual bool wifi_available() const { return false; }
  virtual bool cellular_available() const { return false; }
  virtual bool ethernet_enabled() const { return true; }
  virtual bool wifi_enabled() const { return false; }
  virtual bool cellular_enabled() const { return false; }
  virtual bool wifi_scanning() const { return false; }
  virtual const Network* active_network() const { return NULL; }
  virtual bool offline_mode() const { return false; }
  virtual void EnableEthernetNetworkDevice(bool enable) {}
  virtual void EnableWifiNetworkDevice(bool enable) {}
  virtual void EnableCellularNetworkDevice(bool enable) {}
  virtual void EnableOfflineMode(bool enable) {}
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address) {
    hardware_address->clear();
    return NetworkIPConfigVector();
  }
  virtual std::string GetHtmlInfo(int refresh) { return std::string(); }

 private:
  std::string ip_address_;
  EthernetNetwork* ethernet_;
  WifiNetwork* wifi_;
  CellularNetwork* cellular_;
  WifiNetworkVector wifi_networks_;
  CellularNetworkVector cellular_networks_;
};

// static
NetworkLibrary* NetworkLibrary::GetImpl(bool stub) {
  if (stub)
    return new NetworkLibraryStubImpl();
  else
    return new NetworkLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::NetworkLibraryImpl);
