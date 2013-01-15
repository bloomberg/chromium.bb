// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_
#define CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

class TestMobileActivator;

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
    // Performing pre-activation process.
    PLAN_ACTIVATION_INITIATING_ACTIVATION   = 3,
    // Reconnecting to network.
    PLAN_ACTIVATION_RECONNECTING            = 4,
    // Loading payment portal page.
    PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING  = 5,
    // Showing payment portal page.
    PLAN_ACTIVATION_SHOWING_PAYMENT         = 6,
    // Decides whether to load the portal again or call us done.
    PLAN_ACTIVATION_RECONNECTING_PAYMENT    = 7,
    // Delaying activation until payment portal catches up.
    PLAN_ACTIVATION_DELAY_OTASP             = 8,
    // Starting post-payment activation attempt.
    PLAN_ACTIVATION_START_OTASP             = 9,
    // Attempting activation.
    PLAN_ACTIVATION_OTASP                   = 10,
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
  // Initiates activation process.  Can only be called from the UI thread.
  void InitiateActivation(const std::string& service_path);
  // Terminates activation process if already started.
  void TerminateActivation();
  // Process portal load attempt status.
  void OnPortalLoaded(bool success);
  // Process payment transaction status.
  void OnSetTransactionStatus(bool success);

 private:
  friend struct DefaultSingletonTraits<MobileActivator>;
  friend class TestMobileActivator;
  FRIEND_TEST_ALL_PREFIXES(MobileActivatorTest, BasicFlowForNewDevices);
  FRIEND_TEST_ALL_PREFIXES(MobileActivatorTest, OTASPScheduling);
  FRIEND_TEST_ALL_PREFIXES(MobileActivatorTest,
                           ReconnectOnDisconnectFromPaymentPortal);
  FRIEND_TEST_ALL_PREFIXES(MobileActivatorTest, StartAtStart);
  // We reach directly into the activator for testing purposes.
  friend class MobileActivatorTest;

  MobileActivator();
  virtual ~MobileActivator();

  // NetworkLibrary::NetworkManagerObserver overrides.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;
  // NetworkLibrary::NetworkObserver overrides.
  virtual void OnNetworkChanged(NetworkLibrary* obj,
                                const Network* network) OVERRIDE;

  // Continue activation after inital setup (config load).
  void ContinueActivation();
  // Handles the signal that the payment portal has finished loading.
  void HandlePortalLoaded(bool success);
  // Handles the signal that the user has finished with the portal.
  void HandleSetTransactionStatus(bool success);
  // Starts activation.
  void StartActivation();
  // Called after we delay our OTASP (after payment).
  void RetryOTASP();
  // Continues activation process. This method is called after we disconnect
  // due to detected connectivity issue to kick off reconnection.
  void ContinueConnecting();

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Starts OTASP process.
  void StartOTASP();
  // Called when an OTASP attempt times out.
  void HandleOTASPTimeout();
  // Forces disconnect / reconnect when we detect portal connectivity issues.
  void ForceReconnect(CellularNetwork* network, PlanActivationState next_state);
  // Called when ForceReconnect takes too long to reconnect.
  void ReconnectTimedOut();
  // Verify the state of cellular network and modify internal state.
  virtual void EvaluateCellularNetwork(CellularNetwork* network);
  // PickNextState selects the desired state based on the current state of the
  // modem and the activator.  It does not transition to this state however.
  PlanActivationState PickNextState(CellularNetwork* network,
                                    std::string* error_description) const;
  // One of PickNext*State are called in PickNextState based on whether the
  // modem is online or not.
  PlanActivationState PickNextOnlineState(CellularNetwork* network) const;
  PlanActivationState PickNextOfflineState(CellularNetwork* network) const;
  // Check the current cellular network for error conditions.
  bool GotActivationError(CellularNetwork* network,
                          std::string* error) const;
  // Sends status updates to WebUI page.
  void UpdatePage(CellularNetwork* network,
                  const std::string& error_description);
  // Changes internal state.
  virtual void ChangeState(CellularNetwork* network,
                           PlanActivationState new_state,
                           const std::string& error_description);
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
  std::string GetErrorMessage(const std::string& code) const;

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
  // Finds cellular network that matches |meid_| or |iccid_|, reattach network
  // change observer if |reattach_observer| flag is set.
  virtual CellularNetwork* FindMatchingCellularNetwork(bool reattach_observer);
  // Starts the OTASP timeout timer.  If the timer fires, we'll force a
  // disconnect/reconnect cycle on this network.
  virtual void StartOTASPTimer();

  static const char* GetStateDescription(PlanActivationState state);

  virtual NetworkLibrary* GetNetworkLibrary() const;

  scoped_refptr<CellularConfigDocument> cellular_config_;
  // Internal handler state.
  PlanActivationState state_;
  // MEID of cellular device to activate.
  std::string meid_;
  // ICCID of the SIM card on cellular device to activate.
  std::string iccid_;
  // Service path of network being activated. Note that the path can change
  // during the activation process while still representing the same service.
  std::string service_path_;
  // Flags that controls if cert_checks needs to be restored
  // after the activation of cellular network.
  bool reenable_cert_check_;
  // True if activation process has been terminated.
  bool terminated_;
  // Connection retry counter.
  int connection_retry_count_;
  // Counters for how many times we've tried each OTASP step.
  int initial_OTASP_attempts_;
  int trying_OTASP_attempts_;
  int final_OTASP_attempts_;
  // Payment portal reload/reconnect attempt count.
  int payment_reconnect_count_;
  // Timer that monitors how long we spend in error-prone states.
  base::RepeatingTimer<MobileActivator> state_duration_timer_;

  // State we will return to if we are disconnected.
  PlanActivationState post_reconnect_state_;
  // Called to continue the reconnect attempt.
  base::RepeatingTimer<MobileActivator> continue_reconnect_timer_;
  // Called when the reconnect attempt times out.
  base::OneShotTimer<MobileActivator> reconnect_timeout_timer_;


  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MobileActivator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MOBILE_MOBILE_ACTIVATOR_H_
