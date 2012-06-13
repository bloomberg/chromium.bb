// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile/mobile_activator.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/observer_list_threadsafe.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Cellular configuration file path.
const char kCellularConfigPath[] =
    "/usr/share/chromeos-assets/mobile/mobile_config.json";

// Cellular config file field names.
const char kVersionField[] = "version";
const char kErrorsField[] = "errors";

// Number of times we will retry to restart the activation process in case
// there is no connectivity in the restricted pool.
const int kMaxActivationAttempt = 3;
// Number of times we will retry to reconnect if connection fails.
const int kMaxConnectionRetry = 10;
// Number of times we will retry to reconnect and reload payment portal page.
const int kMaxPortalReconnectCount = 5;
// Number of times we will retry to reconnect if connection fails.
const int kMaxConnectionRetryOTASP = 30;
// Number of times we will retry to reconnect after payment is processed.
const int kMaxReconnectAttemptOTASP = 30;
// Reconnect retry delay (after payment is processed).
const int kPostPaymentReconnectDelayMS = 30000;   // 30s.
// Connection timeout in seconds.
const int kConnectionTimeoutSeconds = 45;
// Reconnect delay.
const int kReconnectDelayMS = 3000;
// Reconnect timer delay.
const int kReconnectTimerDelayMS = 5000;
// Reconnect delay after previous failure.
const int kFailedReconnectDelayMS = 10000;
// Retry delay after failed OTASP attempt.
const int kOTASPRetryDelay = 20000;

// Error codes matching codes defined in the cellular config file.
const char kErrorDefault[] = "default";
const char kErrorBadConnectionPartial[] = "bad_connection_partial";
const char kErrorBadConnectionActivated[] = "bad_connection_activated";
const char kErrorRoamingOnConnection[] = "roaming_connection";
const char kErrorNoEVDO[] = "no_evdo";
const char kErrorRoamingActivation[] = "roaming_activation";
const char kErrorRoamingPartiallyActivated[] = "roaming_partially_activated";
const char kErrorNoService[] = "no_service";
const char kErrorDisabled[] = "disabled";
const char kErrorNoDevice[] = "no_device";
const char kFailedPaymentError[] = "failed_payment";
const char kFailedConnectivity[] = "connectivity";

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// CellularConfigDocument
//
////////////////////////////////////////////////////////////////////////////////
CellularConfigDocument::CellularConfigDocument() {
}

CellularConfigDocument::~CellularConfigDocument() {
}

std::string CellularConfigDocument::GetErrorMessage(const std::string& code) {
  base::AutoLock create(config_lock_);
  ErrorMap::iterator iter = error_map_.find(code);
  if (iter == error_map_.end())
    return code;
  return iter->second;
}

void CellularConfigDocument::LoadCellularConfigFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Load partner customization startup manifest if it is available.
  FilePath config_path(kCellularConfigPath);
  if (!file_util::PathExists(config_path))
    return;

  if (LoadFromFile(config_path)) {
    DVLOG(1) << "Cellular config file loaded: " << kCellularConfigPath;
  } else {
    LOG(ERROR) << "Error loading cellular config file: " <<
        kCellularConfigPath;
  }
}

bool CellularConfigDocument::LoadFromFile(const FilePath& config_path) {
  std::string config;
  if (!file_util::ReadFileToString(config_path, &config))
    return false;

  scoped_ptr<Value> root(
      base::JSONReader::Read(config, base::JSON_ALLOW_TRAILING_COMMAS));
  DCHECK(root.get() != NULL);
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
    LOG(WARNING) << "Bad cellular config file";
    return false;
  }

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());
  if (!root_dict->GetString(kVersionField, &version_)) {
    LOG(WARNING) << "Cellular config file missing version";
    return false;
  }
  ErrorMap error_map;
  DictionaryValue* errors = NULL;
  if (!root_dict->GetDictionary(kErrorsField, &errors))
    return false;
  for (DictionaryValue::key_iterator keys = errors->begin_keys();
       keys != errors->end_keys();
       ++keys) {
    std::string value;
    if (!errors->GetString(*keys, &value)) {
      LOG(WARNING) << "Bad cellular config error value";
      return false;
    }
    error_map.insert(ErrorMap::value_type(*keys, value));
  }
  SetErrorMap(error_map);
  return true;
}

void CellularConfigDocument::SetErrorMap(
    const ErrorMap& map) {
  base::AutoLock create(config_lock_);
  error_map_.clear();
  error_map_.insert(map.begin(), map.end());
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileActivator
//
////////////////////////////////////////////////////////////////////////////////
MobileActivator::MobileActivator()
    : cellular_config_(new CellularConfigDocument()),
      state_(PLAN_ACTIVATION_PAGE_LOADING),
      reenable_wifi_(false),
      reenable_ethernet_(false),
      reenable_cert_check_(false),
      evaluating_(false),
      terminated_(true),
      connection_retry_count_(0),
      reconnect_wait_count_(0),
      payment_reconnect_count_(0),
      activation_attempt_(0) {
}

MobileActivator::~MobileActivator() {
  TerminateActivation();
}

MobileActivator* MobileActivator::GetInstance() {
  return Singleton<MobileActivator>::get();
}

void MobileActivator::TerminateActivation() {
  reconnect_timer_.Stop();
  NetworkLibrary* lib =
      CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  ReEnableOtherConnections();
  meid_.clear();
  service_path_.clear();
  state_ = PLAN_ACTIVATION_PAGE_LOADING;
  reconnect_wait_count_ = 0;
  evaluating_ = false;
  reenable_wifi_ = false;
  reenable_ethernet_ = false;
  reenable_cert_check_ = false;
  terminated_ = true;
  // Release the previous cellular config and setup a new empty one.
  cellular_config_ = new CellularConfigDocument();
}

void MobileActivator::OnNetworkManagerChanged(NetworkLibrary* cros) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  EvaluateCellularNetwork(FindCellularNetworkByMeid(meid_, true));
}

void MobileActivator::OnNetworkChanged(NetworkLibrary* cros,
                                       const Network* network) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;

  if (!network || network->type() != TYPE_CELLULAR) {
    NOTREACHED();
    return;
  }

  EvaluateCellularNetwork(
      static_cast<CellularNetwork*>(const_cast<Network*>(network)));
}

void MobileActivator::AddObserver(MobileActivator::Observer* observer) {
  observers_.AddObserver(observer);
}

void MobileActivator::RemoveObserver(MobileActivator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MobileActivator::InitiateActivation(const std::string& service_path) {
  NetworkLibrary* lib = CrosLibrary::Get()->GetNetworkLibrary();
  CellularNetwork* network = lib->FindCellularNetworkByPath(service_path);
  if (!network) {
    LOG(ERROR) << "Cellular service can't be found: " << service_path;
    return;
  }

  const chromeos::NetworkDevice* device =
      lib->FindNetworkDeviceByPath(network->device_path());
  if (!device) {
    LOG(ERROR) << "Cellular device can't be found: " << network->device_path();
    return;
  }

  terminated_ = false;
  meid_ = device->meid();
  service_path_ = service_path;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&CellularConfigDocument::LoadCellularConfigFile,
                 cellular_config_.get()),
      base::Bind(&MobileActivator::ContinueActivation, AsWeakPtr()));
}

void MobileActivator::ContinueActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, false);
  if (!network || !network->SupportsActivation())
    return;

  DCHECK(!CrosLibrary::Get()->GetNetworkLibrary()->IsLocked());
  SetupActivationProcess(network);

  StartActivation();
}

void MobileActivator::OnSetTransactionStatus(bool success) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MobileActivator::SetTransactionStatus,
                 AsWeakPtr(), success));
}

void MobileActivator::OnPortalLoaded(bool success) {
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, true);
  if (!network) {
    ChangeState(NULL, PLAN_ACTIVATION_ERROR,
                GetErrorMessage(kErrorNoService));
    return;
  }
  if (state_ == PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING ||
      state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    if (success) {
      payment_reconnect_count_ = 0;
      ChangeState(network, PLAN_ACTIVATION_SHOWING_PAYMENT, std::string());
    } else {
      payment_reconnect_count_++;
      if (payment_reconnect_count_ > kMaxPortalReconnectCount) {
        ChangeState(NULL, PLAN_ACTIVATION_ERROR,
                    GetErrorMessage(kErrorNoService));
        return;
      }
      // Disconnect now, this should force reconnection and we will retry to
      // load the frame containing payment portal again.
      DisconnectFromNetwork(network);
    }
  } else {
    NOTREACHED() << "Called paymentPortalLoad while in unexpected state: "
                 << GetStateDescription(state_);
  }
}

CellularNetwork* MobileActivator::FindCellularNetworkByMeid(
    const std::string& meid, bool reattach_observer) {
  NetworkLibrary* lib = CrosLibrary::Get()->GetNetworkLibrary();
  for (CellularNetworkVector::const_iterator it =
           lib->cellular_networks().begin();
       it != lib->cellular_networks().end(); ++it) {
    const chromeos::NetworkDevice* device =
        lib->FindNetworkDeviceByPath((*it)->device_path());
    if (device && meid == device->meid()) {
      CellularNetwork* network = *it;
      // If service path has changed, reattach the event observer for this
      // network service.
      if (reattach_observer && service_path_ != network->service_path()) {
        lib->RemoveObserverForAllNetworks(this);
        lib->AddNetworkObserver(network->service_path(), this);
        service_path_ = network->service_path();
      }
      return network;
    }
  }
  return NULL;
}

void MobileActivator::StartActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_COUNTS("Cellular.MobileSetupStart", 1);
  NetworkLibrary* lib =
      CrosLibrary::Get()->GetNetworkLibrary();
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, true);
  // Check if we can start activation process.
  if (!network) {
    std::string error;
    if (!lib->cellular_available())
      error = kErrorNoDevice;
    else if (!lib->cellular_enabled())
      error = kErrorDisabled;
    else
      error = kErrorNoService;
    ChangeState(NULL, PLAN_ACTIVATION_ERROR, GetErrorMessage(error));
    return;
  }

  // Start monitoring network property changes.
  lib->AddNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  lib->AddNetworkObserver(network->service_path(), this);
  // Try to start with OTASP immediately if we have received payment recently.
  state_ = lib->HasRecentCellularPlanPayment() ?
               PLAN_ACTIVATION_START_OTASP :
               PLAN_ACTIVATION_START;
  EvaluateCellularNetwork(network);
}

void MobileActivator::RetryOTASP() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == PLAN_ACTIVATION_DELAY_OTASP);
  StartOTASP();
}

void MobileActivator::ContinueConnecting(int delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, true);
  if (network && network->connecting_or_connected()) {
    EvaluateCellularNetwork(network);
  } else {
    ConnectToNetwork(network, delay);
  }
}

void MobileActivator::SetTransactionStatus(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (success && state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    NetworkLibrary* lib =
        CrosLibrary::Get()->GetNetworkLibrary();
    lib->SignalCellularPlanPayment();
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentReceived", 1);
    StartOTASP();
  } else {
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentFailed", 1);
  }
}

void MobileActivator::StartOTASP() {
  state_ = PLAN_ACTIVATION_START_OTASP;
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, true);
  if (network &&
      network->connected() &&
      network->activation_state() == ACTIVATION_STATE_ACTIVATED) {
    CrosLibrary::Get()->GetNetworkLibrary()->
        DisconnectFromNetwork(network);
  } else {
    EvaluateCellularNetwork(network);
  }
}

void MobileActivator::ReconnectTimerFired() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Permit network connection changes only in reconnecting states.
  if (state_ != PLAN_ACTIVATION_RECONNECTING_OTASP_TRY &&
      state_ != PLAN_ACTIVATION_RECONNECTING &&
      state_ != PLAN_ACTIVATION_RECONNECTING_PAYMENT &&
      state_ != PLAN_ACTIVATION_RECONNECTING_OTASP)
    return;
  CellularNetwork* network = FindCellularNetworkByMeid(meid_, true);
  if (!network) {
    // No service, try again since this is probably just transient condition.
    LOG(WARNING) << "Service not present at reconnect attempt.";
  }
  EvaluateCellularNetwork(network);
}

void MobileActivator::DisconnectFromNetwork(CellularNetwork* network) {
  DCHECK(network);
  LOG(INFO) << "Disconnecting from: " << network->service_path();
  CrosLibrary::Get()->GetNetworkLibrary()->
      DisconnectFromNetwork(network);
  // Disconnect will force networks to be reevaluated, so
  // we don't want to continue processing on this path anymore.
  evaluating_ = false;
}

bool MobileActivator::NeedsReconnecting(CellularNetwork* network,
                                        PlanActivationState* new_state,
                                        std::string* error_description) {
  DCHECK(network);
  if (!network->failed() && !ConnectionTimeout())
    return false;

  // Try to reconnect again if reconnect failed, or if for some
  // reasons we are still not connected after 45 seconds.
  int max_retries = (state_ == PLAN_ACTIVATION_RECONNECTING_OTASP) ?
                        kMaxConnectionRetryOTASP : kMaxConnectionRetry;
  if (connection_retry_count_ < max_retries) {
    UMA_HISTOGRAM_COUNTS("Cellular.ConnectionRetry", 1);
    ConnectToNetwork(network, kFailedReconnectDelayMS);
    return true;
  }
  // We simply can't connect anymore after all these tries.
  UMA_HISTOGRAM_COUNTS("Cellular.ConnectionFailed", 1);
  *new_state = PLAN_ACTIVATION_ERROR;
  *error_description = GetErrorMessage(kFailedConnectivity);
  return false;
}

bool MobileActivator::ConnectToNetwork(CellularNetwork* network, int delay) {
  if (network && network->connecting_or_connected())
    return true;
  // Permit network connection changes only in reconnecting states.
  if (state_ != PLAN_ACTIVATION_RECONNECTING_OTASP_TRY &&
      state_ != PLAN_ACTIVATION_RECONNECTING &&
      state_ != PLAN_ACTIVATION_RECONNECTING_PAYMENT &&
      state_ != PLAN_ACTIVATION_RECONNECTING_OTASP) return false;
  if (network)
    LOG(INFO) << "Connecting to: " << network->service_path();
  connection_retry_count_++;
  connection_start_time_ = base::Time::Now();
  if (!network || state_ == PLAN_ACTIVATION_RECONNECTING_OTASP) {
    LOG(WARNING) << "Delaying reconnect to "
                 << (network ? network->service_path().c_str() : "no service");
    // If we coudn't connect during reconnection phase, try to reconnect
    // with a delay (and try to reconnect if needed).
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&MobileActivator::ContinueConnecting, AsWeakPtr(), delay),
        base::TimeDelta::FromMilliseconds(delay));
    return false;
  }
  CrosLibrary::Get()->GetNetworkLibrary()->
      ConnectToCellularNetwork(network);
  return true;
}

void MobileActivator::ForceReconnect(CellularNetwork* network, int delay) {
  DCHECK(network);
  UMA_HISTOGRAM_COUNTS("Cellular.ActivationRetry", 1);
  // Reset reconnect metrics.
  connection_retry_count_ = 0;
  connection_start_time_ = base::Time();
  // First, disconnect...
  LOG(INFO) << "Disconnecting from " << network->service_path();
  CrosLibrary::Get()->GetNetworkLibrary()->
      DisconnectFromNetwork(network);
  // Check the network state 3s after we disconnect to make sure.
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MobileActivator::ContinueConnecting, AsWeakPtr(), delay),
      base::TimeDelta::FromMilliseconds(delay));
}

bool MobileActivator::ConnectionTimeout() {
  return (base::Time::Now() -
            connection_start_time_).InSeconds() > kConnectionTimeoutSeconds;
}

void MobileActivator::EvaluateCellularNetwork(CellularNetwork* network) {
  if (terminated_)
    return;

  PlanActivationState new_state = state_;
  if (!network) {
    LOG(WARNING) << "Cellular service lost";
    if (state_ == PLAN_ACTIVATION_RECONNECTING_OTASP_TRY ||
        state_ == PLAN_ACTIVATION_RECONNECTING ||
        state_ == PLAN_ACTIVATION_RECONNECTING_PAYMENT ||
        state_ == PLAN_ACTIVATION_RECONNECTING_OTASP) {
      // This might be the legit case when service is lost after activation.
      // We need to make sure we force reconnection as soon as it shows up.
      LOG(INFO) << "Force service reconnection";
      connection_start_time_ = base::Time();
    }
    return;
  }

  // Prevent this method from being called if it is already on the stack.
  // This might happen on some state transitions (ie. connect, disconnect).
  if (evaluating_)
    return;
  evaluating_ = true;
  std::string error_description;

  LOG(WARNING) << "Cellular:\n  service=" << network->GetStateString().c_str()
      << "\n  ui=" << GetStateDescription(state_)
      << "\n  activation=" << network->GetActivationStateString().c_str()
      << "\n  error=" << network->GetErrorString().c_str()
      << "\n  setvice_path=" << network->service_path().c_str();
  switch (state_) {
    case PLAN_ACTIVATION_START: {
      switch (network->activation_state()) {
        case ACTIVATION_STATE_ACTIVATED: {
          if (network->disconnected()) {
            new_state = PLAN_ACTIVATION_RECONNECTING;
          } else if (network->connected()) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        }
        default: {
          if (network->disconnected() ||
              network->state() == STATE_ACTIVATION_FAILURE) {
            new_state = (network->activation_state() ==
                         ACTIVATION_STATE_PARTIALLY_ACTIVATED) ?
                            PLAN_ACTIVATION_TRYING_OTASP :
                            PLAN_ACTIVATION_INITIATING_ACTIVATION;
          } else if (network->connected()) {
            DisconnectFromNetwork(network);
            return;
          }
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_START_OTASP: {
      switch (network->activation_state()) {
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED: {
          if (network->disconnected()) {
            new_state = PLAN_ACTIVATION_OTASP;
          } else if (network->connected()) {
            DisconnectFromNetwork(network);
            return;
          }
          break;
        }
        case ACTIVATION_STATE_ACTIVATED:
          new_state = PLAN_ACTIVATION_RECONNECTING_OTASP;
          break;
        default: {
          LOG(WARNING) << "Unexpected activation state for device "
                       << network->service_path().c_str();
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_DELAY_OTASP:
      // Just ignore any changes until the OTASP retry timer kicks in.
      evaluating_ = false;
      return;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION: {
      switch (network->activation_state()) {
        case ACTIVATION_STATE_ACTIVATED:
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          new_state = PLAN_ACTIVATION_START;
          break;
        case ACTIVATION_STATE_NOT_ACTIVATED:
        case ACTIVATION_STATE_ACTIVATING:
          // Wait in this state until activation state changes.
          break;
        default:
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_OTASP:
    case PLAN_ACTIVATION_TRYING_OTASP: {
      switch (network->activation_state()) {
        case ACTIVATION_STATE_ACTIVATED:
          if (network->disconnected()) {
            new_state = GetNextReconnectState(state_);
          } else if (network->connected()) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          if (network->connected()) {
            if (network->restricted_pool())
              new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
          } else {
            new_state = GetNextReconnectState(state_);
          }
          break;
        case ACTIVATION_STATE_NOT_ACTIVATED:
        case ACTIVATION_STATE_ACTIVATING:
          // Wait in this state until activation state changes.
          break;
        default:
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
    case PLAN_ACTIVATION_RECONNECTING: {
      if (network->connected()) {
        // Make sure other networks are not interfering with our detection of
        // restricted pool.
        DisableOtherNetworks();
        // Wait until the service shows up and gets activated.
        switch (network->activation_state()) {
          case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          case ACTIVATION_STATE_ACTIVATED:
            if (network->restricted_pool()) {
              if (network->error() == ERROR_DNS_LOOKUP_FAILED) {
                LOG(WARNING) << "No connectivity for device "
                             << network->service_path().c_str();
                // If we are connected but there is no connectivity at all,
                // restart the whole process again.
                if (activation_attempt_ < kMaxActivationAttempt) {
                  activation_attempt_++;
                  LOG(WARNING) << "Reconnect attempt #"
                               << activation_attempt_;
                  ForceReconnect(network, kFailedReconnectDelayMS);
                  evaluating_ = false;
                  return;
                } else {
                  new_state = PLAN_ACTIVATION_ERROR;
                  UMA_HISTOGRAM_COUNTS("Cellular.ActivationRetryFailure", 1);
                  error_description = GetErrorMessage(kFailedConnectivity);
                }
              } else {
                // If we have already received payment, don't show the payment
                // page again. We should try to reconnect after some
                // time instead.
                new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
              }
            } else if (network->activation_state() ==
                           ACTIVATION_STATE_ACTIVATED) {
              new_state = PLAN_ACTIVATION_DONE;
            }
            break;
          default:
            break;
        }
      } else if (NeedsReconnecting(network, &new_state, &error_description)) {
        evaluating_ = false;
        return;
      }
      break;
    }
    case PLAN_ACTIVATION_RECONNECTING_OTASP: {
      if (network->connected()) {
        // Make sure other networks are not interfering with our detection of
        // restricted pool.
        DisableOtherNetworks();
        // Wait until the service shows up and gets activated.
        switch (network->activation_state()) {
          case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          case ACTIVATION_STATE_ACTIVATED:
            if (network->restricted_pool()) {
              LOG(WARNING) << "Still no connectivity after OTASP for device "
                           << network->service_path().c_str();
              // If we have already received payment, don't show the payment
              // page again. We should try to reconnect after some time instead.
              if (reconnect_wait_count_ < kMaxReconnectAttemptOTASP) {
                reconnect_wait_count_++;
                LOG(WARNING) << "OTASP reconnect attempt #"
                             << reconnect_wait_count_;
                ForceReconnect(network, kPostPaymentReconnectDelayMS);
                evaluating_ = false;
                return;
              } else {
                new_state = PLAN_ACTIVATION_ERROR;
                UMA_HISTOGRAM_COUNTS("Cellular.PostPaymentConnectFailure", 1);
                error_description = GetErrorMessage(kFailedConnectivity);
              }
            } else if (network->online()) {
              new_state = PLAN_ACTIVATION_DONE;
            }
            break;
          default:
            break;
        }
      } else if (NeedsReconnecting(network, &new_state, &error_description)) {
        evaluating_ = false;
        return;
      }
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      break;
    // Just ignore all signals until the site confirms payment.
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT: {
      if (network->disconnected())
        new_state = PLAN_ACTIVATION_RECONNECTING_PAYMENT;
      break;
    }
      // Activation completed/failed, ignore network changes.
    case PLAN_ACTIVATION_DONE:
    case PLAN_ACTIVATION_ERROR:
      break;
  }

  if (new_state != PLAN_ACTIVATION_ERROR &&
      GotActivationError(network, &error_description)) {
    // Check for this special case when we try to do activate partially
    // activated device. If that attempt failed, try to disconnect to clear the
    // state and reconnect again.
    if ((network->activation_state() ==
            ACTIVATION_STATE_PARTIALLY_ACTIVATED ||
        network->activation_state() == ACTIVATION_STATE_ACTIVATING) &&
        (network->error() == ERROR_NO_ERROR ||
            network->error() == ERROR_OTASP_FAILED) &&
        network->state() == STATE_ACTIVATION_FAILURE) {
      LOG(WARNING) << "Activation failure detected "
                   << network->service_path().c_str();
      switch (state_) {
        case PLAN_ACTIVATION_OTASP:
        case PLAN_ACTIVATION_RECONNECTING_OTASP:
          new_state = PLAN_ACTIVATION_DELAY_OTASP;
          break;
        case PLAN_ACTIVATION_TRYING_OTASP:
          new_state = PLAN_ACTIVATION_RECONNECTING_OTASP_TRY;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
          new_state = PLAN_ACTIVATION_RECONNECTING;
          break;
        case PLAN_ACTIVATION_START:
          // We are just starting, so this must be previous activation attempt
          // failure.
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        case PLAN_ACTIVATION_DELAY_OTASP:
        case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
        case PLAN_ACTIVATION_RECONNECTING:
          new_state = state_;
          break;
        default:
          new_state = PLAN_ACTIVATION_ERROR;
          break;
      }
    } else {
      LOG(WARNING) << "Unexpected activation failure for "
                   << network->service_path().c_str();
      new_state = PLAN_ACTIVATION_ERROR;
    }
  }

  if (new_state == PLAN_ACTIVATION_ERROR && !error_description.length())
    error_description = GetErrorMessage(kErrorDefault);

  ChangeState(network, new_state, error_description);
  evaluating_ = false;
}

MobileActivator::PlanActivationState MobileActivator::GetNextReconnectState(
    MobileActivator::PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return PLAN_ACTIVATION_RECONNECTING;
    case PLAN_ACTIVATION_OTASP:
      return PLAN_ACTIVATION_RECONNECTING_OTASP;
    case PLAN_ACTIVATION_TRYING_OTASP:
      return PLAN_ACTIVATION_RECONNECTING_OTASP_TRY;
    default:
      return PLAN_ACTIVATION_RECONNECTING;
  }
}

// Debugging helper function, will take it out at the end.
const char* MobileActivator::GetStateDescription(PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_PAGE_LOADING:
      return "PAGE_LOADING";
    case PLAN_ACTIVATION_START:
      return "ACTIVATION_START";
    case PLAN_ACTIVATION_TRYING_OTASP:
      return "TRYING_OTASP";
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
      return "RECONNECTING_OTASP_TRY";
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return "INITIATING_ACTIVATION";
    case PLAN_ACTIVATION_RECONNECTING:
      return "RECONNECTING";
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
      return "PAYMENT_PORTAL_LOADING";
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      return "SHOWING_PAYMENT";
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      return "RECONNECTING_PAYMENT";
    case PLAN_ACTIVATION_START_OTASP:
      return "START_OTASP";
    case PLAN_ACTIVATION_DELAY_OTASP:
      return "DELAY_OTASP";
    case PLAN_ACTIVATION_OTASP:
      return "OTASP";
    case PLAN_ACTIVATION_RECONNECTING_OTASP:
      return "RECONNECTING_OTASP";
    case PLAN_ACTIVATION_DONE:
      return "DONE";
    case PLAN_ACTIVATION_ERROR:
      return "ERROR";
  }
  return "UNKNOWN";
}


void MobileActivator::CompleteActivation(
    CellularNetwork* network) {
  // Remove observers, we are done with this page.
  NetworkLibrary* lib = CrosLibrary::Get()->
      GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  // If we have successfully activated the connection, set autoconnect flag.
  if (network)
    network->SetAutoConnect(true);
  // Reactivate other types of connections if we have
  // shut them down previously.
  ReEnableOtherConnections();
}

bool MobileActivator::RunningActivation() const {
  return !(state_ == PLAN_ACTIVATION_DONE ||
           state_ == PLAN_ACTIVATION_ERROR ||
           state_ == PLAN_ACTIVATION_PAGE_LOADING);
}

void MobileActivator::ChangeState(CellularNetwork* network,
                                  PlanActivationState new_state,
                                  const std::string& error_description) {
  static bool first_time = true;
  if (state_ == new_state && !first_time)
    return;
  LOG(WARNING) << "Activation state flip old = "
      << GetStateDescription(state_)
      << ", new = " << GetStateDescription(new_state);
  first_time = false;

  // Pick action that should happen on leaving the old state.
  switch (state_) {
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING:
    case PLAN_ACTIVATION_RECONNECTING_OTASP:
      if (reconnect_timer_.IsRunning()) {
        reconnect_timer_.Stop();
      }
      break;
    default:
      break;
  }
  state_ = new_state;

  // Signal to observers layer that the state is changing.
  FOR_EACH_OBSERVER(Observer, observers_,
      OnActivationStateChanged(network, state_, error_description));

  // Pick action that should happen on entering the new state.
  switch (new_state) {
    case PLAN_ACTIVATION_START:
      break;
    case PLAN_ACTIVATION_DELAY_OTASP: {
      UMA_HISTOGRAM_COUNTS("Cellular.RetryOTASP", 1);
      BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&MobileActivator::RetryOTASP, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(kOTASPRetryDelay));
      break;
    }
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
    case PLAN_ACTIVATION_TRYING_OTASP:
    case PLAN_ACTIVATION_OTASP:
      DCHECK(network);
      LOG(WARNING) << "Activating service " << network->service_path().c_str();
      UMA_HISTOGRAM_COUNTS("Cellular.ActivationTry", 1);
      if (!network->StartActivation()) {
        UMA_HISTOGRAM_COUNTS("Cellular.ActivationFailure", 1);
        if (new_state == PLAN_ACTIVATION_OTASP) {
          ChangeState(network, PLAN_ACTIVATION_DELAY_OTASP, std::string());
        } else {
          ChangeState(network, PLAN_ACTIVATION_ERROR,
                      GetErrorMessage(kFailedConnectivity));
        }
      }
      break;
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING:
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
    case PLAN_ACTIVATION_RECONNECTING_OTASP: {
      // Start reconnect timer. This will ensure that we are not left in
      // limbo by the network library.
      if (!reconnect_timer_.IsRunning()) {
        reconnect_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(kReconnectTimerDelayMS),
            this, &MobileActivator::ReconnectTimerFired);
      }
      // Reset connection metrics and try to connect.
      reconnect_wait_count_ = 0;
      connection_retry_count_ = 0;
      connection_start_time_ = base::Time::Now();
      ConnectToNetwork(network, kReconnectDelayMS);
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      return;
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PLAN_ACTIVATION_DONE:
      DCHECK(network);
      CompleteActivation(network);
      UMA_HISTOGRAM_COUNTS("Cellular.MobileSetupSucceeded", 1);
      break;
    case PLAN_ACTIVATION_ERROR:
      CompleteActivation(NULL);
      UMA_HISTOGRAM_COUNTS("Cellular.PlanFailed", 1);
      break;
    default:
      break;
  }
}

void MobileActivator::ReEnableOtherConnections() {
  NetworkLibrary* lib = CrosLibrary::Get()->GetNetworkLibrary();
  if (reenable_ethernet_) {
    reenable_ethernet_ = false;
    lib->EnableEthernetNetworkDevice(true);
  }
  if (reenable_wifi_) {
    reenable_wifi_ = false;
    lib->EnableWifiNetworkDevice(true);
  }

  PrefService* prefs = g_browser_process->local_state();
  if (reenable_cert_check_) {
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled,
                      true);
    reenable_cert_check_ = false;
  }
}

void MobileActivator::SetupActivationProcess(CellularNetwork* network) {
  if (!network)
    return;

  // Disable SSL cert checks since we will be doing this in
  // restricted pool.
  PrefService* prefs = g_browser_process->local_state();
  if (!reenable_cert_check_ &&
      prefs->GetBoolean(
          prefs::kCertRevocationCheckingEnabled)) {
    reenable_cert_check_ = true;
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled, false);
  }

  NetworkLibrary* lib = CrosLibrary::Get()->
      GetNetworkLibrary();
  // Disable autoconnect to cellular network.
  network->SetAutoConnect(false);

  // Prevent any other network interference.
  DisableOtherNetworks();
  lib->Lock();
}

void MobileActivator::DisableOtherNetworks() {
  NetworkLibrary* lib = CrosLibrary::Get()->GetNetworkLibrary();
  // Disable ethernet and wifi.
  if (lib->ethernet_enabled()) {
    reenable_ethernet_ = true;
    lib->EnableEthernetNetworkDevice(false);
  }
  if (lib->wifi_enabled()) {
    reenable_wifi_ = true;
    lib->EnableWifiNetworkDevice(false);
  }
}

bool MobileActivator::GotActivationError(
    CellularNetwork* network, std::string* error) {
  DCHECK(network);
  bool got_error = false;
  const char* error_code = kErrorDefault;

  // This is the magic for detection of errors in during activation process.
  if (network->state() == STATE_FAILURE &&
      network->error() == ERROR_AAA_FAILED) {
    if (network->activation_state() ==
            ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
      error_code = kErrorBadConnectionPartial;
    } else if (network->activation_state() ==
            ACTIVATION_STATE_ACTIVATED) {
      if (network->roaming_state() == ROAMING_STATE_HOME) {
        error_code = kErrorBadConnectionActivated;
      } else if (network->roaming_state() == ROAMING_STATE_ROAMING) {
        error_code = kErrorRoamingOnConnection;
      }
    }
    got_error = true;
  } else if (network->state() == STATE_ACTIVATION_FAILURE) {
    if (network->error() == ERROR_NEED_EVDO) {
      if (network->activation_state() ==
              ACTIVATION_STATE_PARTIALLY_ACTIVATED)
        error_code = kErrorNoEVDO;
    } else if (network->error() == ERROR_NEED_HOME_NETWORK) {
      if (network->activation_state() ==
              ACTIVATION_STATE_NOT_ACTIVATED) {
        error_code = kErrorRoamingActivation;
      } else if (network->activation_state() ==
                    ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        error_code = kErrorRoamingPartiallyActivated;
      }
    }
    got_error = true;
  }

  if (got_error)
    *error = GetErrorMessage(error_code);

  return got_error;
}

void MobileActivator::GetDeviceInfo(CellularNetwork* network,
                                     DictionaryValue* value) {
  DCHECK(network);
  NetworkLibrary* cros =
      CrosLibrary::Get()->GetNetworkLibrary();
  if (!cros)
    return;
  value->SetString("carrier", network->name());
  value->SetString("payment_url", network->payment_url());
  if (network->using_post() && network->post_data().length())
    value->SetString("post_data", network->post_data());

  const NetworkDevice* device =
      cros->FindNetworkDeviceByPath(network->device_path());
  if (device) {
    value->SetString("MEID", device->meid());
    value->SetString("IMEI", device->imei());
    value->SetString("MDN", device->mdn());
  }
}

std::string MobileActivator::GetErrorMessage(const std::string& code) {
  return cellular_config_->GetErrorMessage(code);
}

}  // namespace chromeos
