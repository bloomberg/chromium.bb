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

// Number of times we'll try an OTASP before failing the activation process.
const int kMaxOTASPTries = 3;
// Number of times we will retry to reconnect and reload payment portal page.
const int kMaxPortalReconnectCount = 2;
// Time between connection attempts when forcing a reconnect.
const int kReconnectDelayMS = 3000;
// Retry delay after failed OTASP attempt.
const int kOTASPRetryDelay = 40000;
// Maximum amount of time we'll wait for a service to reconnect.
const int kMaxReconnectTime = 30000;

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
CellularConfigDocument::CellularConfigDocument() {}

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

  if (LoadFromFile(config_path))
    DVLOG(1) << "Cellular config file loaded: " << kCellularConfigPath;
  else
    LOG(ERROR) << "Error loading cellular config file: " << kCellularConfigPath;
}

CellularConfigDocument::~CellularConfigDocument() {}

void CellularConfigDocument::SetErrorMap(
    const ErrorMap& map) {
  base::AutoLock create(config_lock_);
  error_map_.clear();
  error_map_.insert(map.begin(), map.end());
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

////////////////////////////////////////////////////////////////////////////////
//
// MobileActivator
//
////////////////////////////////////////////////////////////////////////////////
MobileActivator::MobileActivator()
    : cellular_config_(new CellularConfigDocument()),
      state_(PLAN_ACTIVATION_PAGE_LOADING),
      reenable_cert_check_(false),
      terminated_(true),
      connection_retry_count_(0),
      payment_reconnect_count_(0) {
}

MobileActivator::~MobileActivator() {
  TerminateActivation();
}

MobileActivator* MobileActivator::GetInstance() {
  return Singleton<MobileActivator>::get();
}

void MobileActivator::TerminateActivation() {
  // We're exiting; don't continue with termination.
  if (!CrosLibrary::Get())
    return;

  state_duration_timer_.Stop();
  continue_reconnect_timer_.Stop();
  reconnect_timeout_timer_.Stop();

  NetworkLibrary* lib = GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  ReEnableCertRevocationChecking();
  meid_.clear();
  iccid_.clear();
  service_path_.clear();
  state_ = PLAN_ACTIVATION_PAGE_LOADING;
  reenable_cert_check_ = false;
  terminated_ = true;
  // Release the previous cellular config and setup a new empty one.
  cellular_config_ = new CellularConfigDocument();
}

void MobileActivator::OnNetworkManagerChanged(NetworkLibrary* cros) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  EvaluateCellularNetwork(FindMatchingCellularNetwork(true));
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
  DCHECK(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void MobileActivator::RemoveObserver(MobileActivator::Observer* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void MobileActivator::InitiateActivation(const std::string& service_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  NetworkLibrary* lib = GetNetworkLibrary();
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
  iccid_ = device->iccid();
  service_path_ = service_path;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&CellularConfigDocument::LoadCellularConfigFile,
                 cellular_config_.get()),
      base::Bind(&MobileActivator::ContinueActivation, AsWeakPtr()));
}

void MobileActivator::ContinueActivation() {
  CellularNetwork* network = FindMatchingCellularNetwork(false);
  if (!network || !network->SupportsActivation())
    return;

  NetworkLibrary* lib = GetNetworkLibrary();

  DisableCertRevocationChecking();
  // We want shill to connect us after activations.
  network->SetAutoConnect(true);

  DCHECK(!lib->IsLocked());
  lib->Lock();

  StartActivation();
}

void MobileActivator::OnSetTransactionStatus(bool success) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MobileActivator::HandleSetTransactionStatus,
                 AsWeakPtr(), success));
}

void MobileActivator::HandleSetTransactionStatus(bool success) {
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (success && state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    NetworkLibrary* lib = GetNetworkLibrary();
    lib->SignalCellularPlanPayment();
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentReceived", 1);
    CellularNetwork* network = FindMatchingCellularNetwork(true);
    if (network && network->activate_over_non_cellular_network()) {
      state_ = PLAN_ACTIVATION_DONE;
      EvaluateCellularNetwork(network);
    } else {
      StartOTASP();
    }
  } else {
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentFailed", 1);
  }
}

void MobileActivator::OnPortalLoaded(bool success) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MobileActivator::HandlePortalLoaded,
                 AsWeakPtr(), success));
}

void MobileActivator::HandlePortalLoaded(bool success) {
  CellularNetwork* network = FindMatchingCellularNetwork(true);
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

      // Reconnect and try and load the frame again.
      ChangeState(network,
                  PLAN_ACTIVATION_RECONNECTING,
                  GetErrorMessage(kFailedPaymentError));
    }
  } else {
    NOTREACHED() << "Called paymentPortalLoad while in unexpected state: "
                 << GetStateDescription(state_);
  }
}

CellularNetwork* MobileActivator::FindMatchingCellularNetwork(
    bool reattach_observer) {
  NetworkLibrary* lib = GetNetworkLibrary();
  for (CellularNetworkVector::const_iterator it =
           lib->cellular_networks().begin();
       it != lib->cellular_networks().end(); ++it) {
    const chromeos::NetworkDevice* device =
        lib->FindNetworkDeviceByPath((*it)->device_path());
    if (device && ((!meid_.empty() && meid_ == device->meid()) ||
                   (!iccid_.empty() && iccid_ == device->iccid()))) {
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

void MobileActivator::StartOTASPTimer() {
  state_duration_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kOTASPRetryDelay),
      this, &MobileActivator::HandleOTASPTimeout);
}

void MobileActivator::StartActivation() {
  UMA_HISTOGRAM_COUNTS("Cellular.MobileSetupStart", 1);
  NetworkLibrary* lib = GetNetworkLibrary();
  CellularNetwork* network = FindMatchingCellularNetwork(true);
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
  if (network->activate_over_non_cellular_network()) {
    // Fast forward to payment portal loading if the activation is performed
    // over a non-cellular network.
    ChangeState(network,
                (network->activation_state() == ACTIVATION_STATE_ACTIVATED) ?
                PLAN_ACTIVATION_DONE :
                PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING,
                "");
  } else if (lib->HasRecentCellularPlanPayment() &&
             network->activation_state() ==
                 ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
    // Try to start with OTASP immediately if we have received payment recently.
    state_ = PLAN_ACTIVATION_START_OTASP;
  } else {
    state_ =  PLAN_ACTIVATION_START;
  }

  EvaluateCellularNetwork(network);
}

void MobileActivator::RetryOTASP() {
  DCHECK(state_ == PLAN_ACTIVATION_DELAY_OTASP);
  StartOTASP();
}

void MobileActivator::StartOTASP() {
  CellularNetwork* network = FindMatchingCellularNetwork(true);
  ChangeState(network, PLAN_ACTIVATION_START_OTASP, std::string());
  EvaluateCellularNetwork(network);
}

void MobileActivator::HandleOTASPTimeout() {
  LOG(WARNING) << "OTASP seems to be taking too long.";
  CellularNetwork* network = FindMatchingCellularNetwork(true);
  // We're here because one of OTASP steps is taking too long to complete.
  // Usually, this means something bad has happened below us.
  if (state_ == PLAN_ACTIVATION_INITIATING_ACTIVATION) {
    ++initial_OTASP_attempts_;
    if (initial_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network,
                  PLAN_ACTIVATION_RECONNECTING,
                  GetErrorMessage(kErrorDefault));
      return;
    }
  } else if (state_ == PLAN_ACTIVATION_TRYING_OTASP) {
    ++trying_OTASP_attempts_;
    if (trying_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network,
                  PLAN_ACTIVATION_RECONNECTING,
                  GetErrorMessage(kErrorDefault));
      return;
    }
  } else if (state_ == PLAN_ACTIVATION_OTASP) {
    ++final_OTASP_attempts_;
    if (final_OTASP_attempts_ <= kMaxOTASPTries) {
      // Give the portal time to propagate all those magic bits.
      ChangeState(network,
                  PLAN_ACTIVATION_DELAY_OTASP,
                  GetErrorMessage(kErrorDefault));
      return;
    }
  } else {
    LOG(ERROR) << "OTASP timed out from a non-OTASP wait state?";
  }
  LOG(ERROR) << "OTASP failed too many times; aborting.";
  ChangeState(network,
              PLAN_ACTIVATION_ERROR,
              GetErrorMessage(kErrorDefault));
}

void MobileActivator::ForceReconnect(CellularNetwork* network,
                                     PlanActivationState next_state) {
  DCHECK(network);
  // Store away our next destination for when we complete.
  post_reconnect_state_ = next_state;
  UMA_HISTOGRAM_COUNTS("Cellular.ActivationRetry", 1);
  // First, disconnect...
  LOG(INFO) << "Disconnecting from " << network->service_path();
  // Explicit service Disconnect()s disable autoconnect on the service until
  // Connect() is called on the service again.  Hence this dance to explicitly
  // call Connect().
  GetNetworkLibrary()->DisconnectFromNetwork(network);
  // Keep trying to connect until told otherwise.
  continue_reconnect_timer_.Stop();
  continue_reconnect_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kReconnectDelayMS),
      this, &MobileActivator::ContinueConnecting);
  // If we don't ever connect again, we're going to call this a failure.
  reconnect_timeout_timer_.Stop();
  reconnect_timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMaxReconnectTime),
      this, &MobileActivator::ReconnectTimedOut);
}

void MobileActivator::ReconnectTimedOut() {
  LOG(ERROR) << "Ending activation attempt after failing to reconnect.";
  ChangeState(FindMatchingCellularNetwork(true),
              PLAN_ACTIVATION_ERROR,
              GetErrorMessage(kFailedConnectivity));
}

void MobileActivator::ContinueConnecting() {
  CellularNetwork* network = FindMatchingCellularNetwork(true);
  if (network && network->connected()) {
    if (network->restricted_pool() &&
        network->error() == ERROR_DNS_LOOKUP_FAILED) {
      // It isn't an error to be in a restricted pool, but if DNS doesn't work,
      // then we're not getting traffic through at all.  Just disconnect and
      // try again.
      GetNetworkLibrary()->DisconnectFromNetwork(network);
      return;
    }
    // Stop this callback
    continue_reconnect_timer_.Stop();
    EvaluateCellularNetwork(network);
  } else {
    LOG(WARNING) << "Connect failed, will try again in a little bit.";
    if (network) {
      LOG(INFO) << "Connecting to: " << network->service_path();
      GetNetworkLibrary()->ConnectToCellularNetwork(network);
    }
  }
}

void MobileActivator::EvaluateCellularNetwork(CellularNetwork* network) {
  if (terminated_) {
    LOG(ERROR) << "Tried to run MobileActivator state machine while "
               << "terminated.";
    return;
  }

  if (!network) {
    LOG(WARNING) << "Cellular service lost";
    return;
  }

  LOG(WARNING) << "Cellular:\n  service=" << network->GetStateString()
      << "\n  ui=" << GetStateDescription(state_)
      << "\n  activation=" << network->GetActivationStateString()
      << "\n  error=" << network->GetErrorString()
      << "\n  setvice_path=" << network->service_path()
      << "\n  connected=" << network->connected();

  std::string error_description;
  PlanActivationState new_state = PickNextState(network, &error_description);

  ChangeState(network, new_state, error_description);
}

MobileActivator::PlanActivationState MobileActivator::PickNextState(
    CellularNetwork* network, std::string* error_description) const {
  PlanActivationState new_state = state_;
  if (!network->connected())
    new_state = PickNextOfflineState(network);
  else
    new_state = PickNextOnlineState(network);
  if (new_state != PLAN_ACTIVATION_ERROR &&
      GotActivationError(network, error_description)) {
    // Check for this special case when we try to do activate partially
    // activated device. If that attempt failed, try to disconnect to clear the
    // state and reconnect again.
    if ((network->activation_state() == ACTIVATION_STATE_PARTIALLY_ACTIVATED ||
         network->activation_state() == ACTIVATION_STATE_ACTIVATING) &&
        (network->error() == ERROR_NO_ERROR ||
         network->error() == ERROR_OTASP_FAILED) &&
        network->state() == STATE_ACTIVATION_FAILURE) {
      LOG(WARNING) << "Activation failure detected "
                   << network->service_path();
      switch (state_) {
        case PLAN_ACTIVATION_OTASP:
          new_state = PLAN_ACTIVATION_DELAY_OTASP;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case PLAN_ACTIVATION_TRYING_OTASP:
          new_state = PLAN_ACTIVATION_START;
          break;
        case PLAN_ACTIVATION_START:
          // We are just starting, so this must be previous activation attempt
          // failure.
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        case PLAN_ACTIVATION_DELAY_OTASP:
          new_state = state_;
          break;
        default:
          new_state = PLAN_ACTIVATION_ERROR;
          break;
      }
    } else {
      LOG(WARNING) << "Unexpected activation failure for "
                   << network->service_path();
      new_state = PLAN_ACTIVATION_ERROR;
    }
  }

  if (new_state == PLAN_ACTIVATION_ERROR && !error_description->length())
    *error_description = GetErrorMessage(kErrorDefault);
  return new_state;
}

MobileActivator::PlanActivationState MobileActivator::PickNextOfflineState(
    CellularNetwork* network) const {
  PlanActivationState new_state = state_;
  switch (state_) {
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      if (!network->activate_over_non_cellular_network())
        new_state = PLAN_ACTIVATION_RECONNECTING;
      break;
    case PLAN_ACTIVATION_START:
      switch (network->activation_state()) {
        case ACTIVATION_STATE_ACTIVATED:
          if (network->restricted_pool())
            new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
          else
            new_state = PLAN_ACTIVATION_DONE;
          break;
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        default:
          new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
          break;
      }
      break;
    default: {
      LOG(INFO) << "Waiting for cellular service to connect.";
      } break;
  }
  return new_state;
}

MobileActivator::PlanActivationState MobileActivator::PickNextOnlineState(
    CellularNetwork* network) const {
  PlanActivationState new_state = state_;
  switch (state_) {
    case PLAN_ACTIVATION_START:
      switch (network->activation_state()) {
        case ACTIVATION_STATE_ACTIVATED:
          if (network->restricted_pool())
            new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
          else
            new_state = PLAN_ACTIVATION_DONE;
          break;
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        default:
          new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
          break;
      }
      break;
    case PLAN_ACTIVATION_START_OTASP: {
      switch (network->activation_state()) {
        case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          new_state = PLAN_ACTIVATION_OTASP;
          break;
        case ACTIVATION_STATE_ACTIVATED:
          new_state = PLAN_ACTIVATION_RECONNECTING;
          break;
        default: {
          LOG(WARNING) << "Unexpected activation state for device "
                       << network->service_path();
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_DELAY_OTASP:
      // Just ignore any changes until the OTASP retry timer kicks in.
      break;
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
          LOG(WARNING) << "Unknown transition";
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_OTASP:
    case PLAN_ACTIVATION_TRYING_OTASP:
      if (network->activation_state() == ACTIVATION_STATE_NOT_ACTIVATED ||
          network->activation_state() == ACTIVATION_STATE_ACTIVATING) {
        LOG(INFO) << "Waiting for the OTASP to finish and the service to "
                  << "come back online";
      } else if (network->activation_state() == ACTIVATION_STATE_ACTIVATED) {
        new_state = PLAN_ACTIVATION_DONE;
      } else {
        new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
      }
      break;
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      if (!network->restricted_pool() &&
          network->activation_state() == ACTIVATION_STATE_ACTIVATED)
        // We're not portalled, and we're already activated, so we're online!
        new_state = PLAN_ACTIVATION_DONE;
      else
        new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
      break;
    // Initial state
    case PLAN_ACTIVATION_PAGE_LOADING:
      break;
    // Just ignore all signals until the site confirms payment.
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      break;
    // Go where we decided earlier.
    case PLAN_ACTIVATION_RECONNECTING:
      new_state = post_reconnect_state_;
      break;
    // Activation completed/failed, ignore network changes.
    case PLAN_ACTIVATION_DONE:
    case PLAN_ACTIVATION_ERROR:
      break;
  }

  return new_state;
}

// Debugging helper function, will take it out at the end.
const char* MobileActivator::GetStateDescription(PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_PAGE_LOADING:
      return "PAGE_LOADING";
    case PLAN_ACTIVATION_START:
      return "ACTIVATION_START";
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return "INITIATING_ACTIVATION";
    case PLAN_ACTIVATION_TRYING_OTASP:
      return "TRYING_OTASP";
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
      return "PAYMENT_PORTAL_LOADING";
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      return "SHOWING_PAYMENT";
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      return "RECONNECTING_PAYMENT";
    case PLAN_ACTIVATION_DELAY_OTASP:
      return "DELAY_OTASP";
    case PLAN_ACTIVATION_START_OTASP:
      return "START_OTASP";
    case PLAN_ACTIVATION_OTASP:
      return "OTASP";
    case PLAN_ACTIVATION_DONE:
      return "DONE";
    case PLAN_ACTIVATION_ERROR:
      return "ERROR";
    case PLAN_ACTIVATION_RECONNECTING:
      return "RECONNECTING";
  }
  return "UNKNOWN";
}


void MobileActivator::CompleteActivation(
    CellularNetwork* network) {
  // Remove observers, we are done with this page.
  NetworkLibrary* lib = GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  // Reactivate other types of connections if we have
  // shut them down previously.
  ReEnableCertRevocationChecking();
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
  LOG(INFO) << "Activation state flip old = "
            << GetStateDescription(state_)
            << ", new = " << GetStateDescription(new_state);
  if (state_ == new_state && !first_time)
    return;
  first_time = false;
  LOG(INFO) << "Transitioning...";

  // Kill all the possible timers and callbacks we might have outstanding.
  state_duration_timer_.Stop();
  continue_reconnect_timer_.Stop();
  reconnect_timeout_timer_.Stop();
  const PlanActivationState old_state = state_;
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
    case PLAN_ACTIVATION_START_OTASP:
      break;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
    case PLAN_ACTIVATION_TRYING_OTASP:
    case PLAN_ACTIVATION_OTASP:
      DCHECK(network);
      LOG(WARNING) << "Activating service " << network->service_path();
      UMA_HISTOGRAM_COUNTS("Cellular.ActivationTry", 1);
      if (!network->StartActivation()) {
        UMA_HISTOGRAM_COUNTS("Cellular.ActivationFailure", 1);
        LOG(ERROR) << "Failed to call Activate() on service in shill.";
        if (new_state == PLAN_ACTIVATION_OTASP) {
          ChangeState(network, PLAN_ACTIVATION_DELAY_OTASP, std::string());
        } else {
          ChangeState(network,
                      PLAN_ACTIVATION_ERROR,
                      GetErrorMessage(kFailedConnectivity));
        }
      } else {
        StartOTASPTimer();
      }
      break;
    case PLAN_ACTIVATION_PAGE_LOADING:
      return;
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PLAN_ACTIVATION_RECONNECTING: {
      PlanActivationState next_state = old_state;
      // Pick where we want to return to after we reconnect.
      switch (old_state) {
        case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
        case PLAN_ACTIVATION_SHOWING_PAYMENT:
          // We decide here what to do next based on the state of the modem.
          next_state = PLAN_ACTIVATION_RECONNECTING_PAYMENT;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case PLAN_ACTIVATION_TRYING_OTASP:
          next_state = PLAN_ACTIVATION_START;
          break;
        case PLAN_ACTIVATION_START_OTASP:
        case PLAN_ACTIVATION_OTASP:
          if (!network || !network->connected()) {
            next_state = PLAN_ACTIVATION_START_OTASP;
          } else {
            // We're online, which means we've conspired with
            // PickNextOnlineState to reconnect after activation (that's the
            // only way we see this transition).  Thus, after we reconnect, we
            // should be done.
            next_state = PLAN_ACTIVATION_DONE;
          }
          break;
        default:
          LOG(ERROR) << "Transitioned to RECONNECTING from an unexpected "
                     << "state.";
          break;
      }
      ForceReconnect(network, next_state);
      break;
    }
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

void MobileActivator::ReEnableCertRevocationChecking() {
  PrefService* prefs = g_browser_process->local_state();
  if (reenable_cert_check_) {
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled,
                      true);
    reenable_cert_check_ = false;
  }
}

void MobileActivator::DisableCertRevocationChecking() {
  // Disable SSL cert checks since we might be performing activation in the
  // restricted pool.
  // TODO(rkc): We want to do this only if on Cellular.
  PrefService* prefs = g_browser_process->local_state();
  if (!reenable_cert_check_ &&
      prefs->GetBoolean(
          prefs::kCertRevocationCheckingEnabled)) {
    reenable_cert_check_ = true;
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled, false);
  }
}

bool MobileActivator::GotActivationError(
    CellularNetwork* network, std::string* error) const {
  DCHECK(network);
  bool got_error = false;
  const char* error_code = kErrorDefault;

  // This is the magic for detection of errors in during activation process.
  if (network->state() == STATE_FAILURE &&
      network->error() == ERROR_AAA_FAILED) {
    if (network->activation_state() ==
            ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
      error_code = kErrorBadConnectionPartial;
    } else if (network->activation_state() == ACTIVATION_STATE_ACTIVATED) {
      if (network->roaming_state() == ROAMING_STATE_HOME)
        error_code = kErrorBadConnectionActivated;
      else if (network->roaming_state() == ROAMING_STATE_ROAMING)
        error_code = kErrorRoamingOnConnection;
    }
    got_error = true;
  } else if (network->state() == STATE_ACTIVATION_FAILURE) {
    if (network->error() == ERROR_NEED_EVDO) {
      if (network->activation_state() == ACTIVATION_STATE_PARTIALLY_ACTIVATED)
        error_code = kErrorNoEVDO;
    } else if (network->error() == ERROR_NEED_HOME_NETWORK) {
      if (network->activation_state() == ACTIVATION_STATE_NOT_ACTIVATED) {
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
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
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

std::string MobileActivator::GetErrorMessage(const std::string& code) const {
  return cellular_config_->GetErrorMessage(code);
}

NetworkLibrary* MobileActivator::GetNetworkLibrary() const {
  return CrosLibrary::Get()->GetNetworkLibrary();
}

}  // namespace chromeos
