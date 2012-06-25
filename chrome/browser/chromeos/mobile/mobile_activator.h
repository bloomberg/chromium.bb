// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_
#define CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_
#pragma once

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Cellular plan config document.
class CellularConfigDocument
    : public base::RefCountedThreadSafe<CellularConfigDocument> {
 public:
  CellularConfigDocument();

  // Return error message for a given code.
  std::string GetErrorMessage(const std::string& code);
  void LoadCellularConfigFile();
  const std::string& version() { return version_; }

 private:
  friend class base::RefCountedThreadSafe<CellularConfigDocument>;
  typedef std::map<std::string, std::string> ErrorMap;

  virtual ~CellularConfigDocument();

  void SetErrorMap(const ErrorMap& map);
  bool LoadFromFile(const FilePath& config_path);

  std::string version_;
  ErrorMap error_map_;
  base::Lock config_lock_;

  DISALLOW_COPY_AND_ASSIGN(CellularConfigDocument);
};

// This class performs mobile plan activation process.
class MobileActivator
  : public NetworkLibrary::NetworkManagerObserver,
    public NetworkLibrary::NetworkObserver,
    public base::SupportsWeakPtr<MobileActivator> {
 public:
  // Activation state.
  enum PlanActivationState {
    // Activation WebUI page is loading, activation not started.
    PLAN_ACTIVATION_PAGE_LOADING            = -1,
    // Activation process started.
    PLAN_ACTIVATION_START                   = 0,
    // Initial over the air activation attempt.
    PLAN_ACTIVATION_TRYING_OTASP            = 1,
    // Reconnection after the initial activation attempt.
    PLAN_ACTIVATION_RECONNECTING_OTASP_TRY  = 2,
    // Performing pre-activation process.
    PLAN_ACTIVATION_INITIATING_ACTIVATION   = 3,
    // Reconnecting to network.
    PLAN_ACTIVATION_RECONNECTING            = 4,
    // Loading payment portal page.
    PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING  = 5,
    // Showing payment portal page.
    PLAN_ACTIVATION_SHOWING_PAYMENT         = 6,
    // Reconnecting after successful plan payment.
    PLAN_ACTIVATION_RECONNECTING_PAYMENT    = 7,
    // Delaying activation until payment portal catches up.
    PLAN_ACTIVATION_DELAY_OTASP             = 8,
    // Starting post-payment activation attempt.
    PLAN_ACTIVATION_START_OTASP             = 9,
    // Attempting activation.
    PLAN_ACTIVATION_OTASP                   = 10,
    // Reconnecting after activation attempt.
    PLAN_ACTIVATION_RECONNECTING_OTASP      = 11,
    // Finished activation.
    PLAN_ACTIVATION_DONE                    = 12,
    // Error occured during activation process.
    PLAN_ACTIVATION_ERROR                   = 0xFF,
  };

  // Activation process observer.
  class Observer {
   public:
    // Signals activation |state| change for given |network|.
    virtual void OnActivationStateChanged(
        CellularNetwork* network,
        PlanActivationState state,
        const std::string& error_description) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  static MobileActivator* GetInstance();

  // Add/remove activation process observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Activation is in process.
  bool RunningActivation() const;
  // Activation state.
  PlanActivationState state() const { return state_; }
  // Initiates activation process.
  void InitiateActivation(const std::string& service_path);
  // Terminates activation process if already started.
  void TerminateActivation();
  // Process portal load attempt status.
  void OnPortalLoaded(bool success);
  // Process payment transaction status.
  void OnSetTransactionStatus(bool success);

 private:
  friend struct DefaultSingletonTraits<MobileActivator>;

  MobileActivator();
  virtual ~MobileActivator();

  // NetworkLibrary::NetworkManagerObserver overrides.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;
  // NetworkLibrary::NetworkObserver overrides.
  virtual void OnNetworkChanged(NetworkLibrary* obj,
                                const Network* network) OVERRIDE;

  // Continue activation after inital setup (config load).
  void ContinueActivation();
  // Process payment transaction results.
  void SetTransactionStatus(bool success);
  // Starts activation.
  void StartActivation();
  // Retried OTASP.
  void RetryOTASP();
  // Continues activation process. This method is called after we disconnect
  // due to detected connectivity issue to kick off reconnection.
  void ContinueConnecting(int delay);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Callback for when |reconnect_timer_| fires.
  void ReconnectTimerFired();
  // Starts OTASP process.
  void StartOTASP();
  // Checks if we need to reconnect due to failed connection attempt.
  bool NeedsReconnecting(CellularNetwork* network,
                         PlanActivationState* new_state,
                         std::string* error_description);
  // Disconnect from network.
  void DisconnectFromNetwork(CellularNetwork* network);
  // Connects to cellular network, resets connection timer.
  bool ConnectToNetwork(CellularNetwork* network, int delay);
  // Forces disconnect / reconnect when we detect portal connectivity issues.
  void ForceReconnect(CellularNetwork* network, int delay);
  // Reports connection timeout.
  bool ConnectionTimeout();
  // Verify the state of cellular network and modify internal state.
  void EvaluateCellularNetwork(CellularNetwork* network);
  // Check the current cellular network for error conditions.
  bool GotActivationError(CellularNetwork* network,
                          std::string* error);
  // Sends status updates to WebUI page.
  void UpdatePage(CellularNetwork* network,
                  const std::string& error_description);
  // Changes internal state.
  void ChangeState(CellularNetwork* network,
                   PlanActivationState new_state,
                   const std::string& error_description);
  // Prepares network devices for cellular activation process.
  void SetupActivationProcess(CellularNetwork* network);
  // Resets network devices after cellular activation process.
  // |network| should be NULL if the activation process failed.
  void CompleteActivation(CellularNetwork* network);
  // Disables SSL certificate revocation checking mechanism. In the case
  // where captive portal connection is the only one present, such revocation
  // checks could prevent payment portal page from loading.
  void DisableCertRevocationChecking();
  // Reenables SSL certificate revocation checking mechanism.
  void ReEnableCertRevocationChecking();
  // Return error message for a given code.
  std::string GetErrorMessage(const std::string& code);

  // Converts the currently active CellularNetwork device into a JS object.
  static void GetDeviceInfo(CellularNetwork* network,
                            DictionaryValue* value);
  static bool ShouldReportDeviceState(std::string* state, std::string* error);

  // Performs activation state cellular device evaluation.
  // Returns false if device activation failed. In this case |error|
  // will contain error message to be reported to Web UI.
  static bool EvaluateCellularDeviceState(bool* report_status,
                                          std::string* state,
                                          std::string* error);
  // Finds cellular network given device |meid|, reattach network change
  // observer if |reattach_observer| flag is set.
  CellularNetwork* FindCellularNetworkByMeid(const std::string& meid,
                                             bool reattach_observer);

  // Returns next reconnection state based on the current activation phase.
  static PlanActivationState GetNextReconnectState(PlanActivationState state);
  static const char* GetStateDescription(PlanActivationState state);

  scoped_refptr<CellularConfigDocument> cellular_config_;
  // Internal handler state.
  PlanActivationState state_;
  // MEID of cellular device to activate.
  std::string meid_;
  // Service path of network begin activated. Please note that the path can
  // change during the activation process even though it is still representing
  // the same service.
  std::string service_path_;
  // Flags that controls if cert_checks needs to be restored
  // after the activation of cellular network.
  bool reenable_cert_check_;
  bool evaluating_;
  // True if we think that another tab is already running activation.
  bool already_running_;
  // True activation process had been terminated.
  bool terminated_;
  // Connection retry counter.
  int connection_retry_count_;
  // Post payment reconnect wait counters.
  int reconnect_wait_count_;
  // Payment portal reload/reconnect attempt count.
  int payment_reconnect_count_;
  // Activation retry attempt count;
  int activation_attempt_;
  // Connection start time.
  base::Time connection_start_time_;
  // Timer that monitors reconnection timeouts.
  base::RepeatingTimer<MobileActivator> reconnect_timer_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MobileActivator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_
