// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "googleurl/src/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// This class handles all notifications about network changes from
// NetworkLibrary and delegates portal detection for the active
// network to CaptivePortalService.
class NetworkPortalDetector
    : public base::NonThreadSafe,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public chromeos::NetworkLibrary::NetworkObserver {
 public:
  enum CaptivePortalState {
    CAPTIVE_PORTAL_STATE_UNKNOWN  = 0,
    CAPTIVE_PORTAL_STATE_OFFLINE  = 1,
    CAPTIVE_PORTAL_STATE_ONLINE   = 2,
    CAPTIVE_PORTAL_STATE_PORTAL   = 3,
  };

  class Observer {
   public:
    // Called when portal state is changed for |network|.
    virtual void OnPortalStateChanged(const Network* network,
                                      CaptivePortalState state) = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~NetworkPortalDetector();

  void Init();
  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  CaptivePortalState GetCaptivePortalState(const chromeos::Network* network);

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros) OVERRIDE;

  // NetworkLibrary::NetworkObserver implementation:
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                const chromeos::Network* network) OVERRIDE;

  // Creates an instance of the NetworkPortalDetector.
  static NetworkPortalDetector* CreateInstance();

  // Gets the instance of the NetworkPortalDetector.
  static NetworkPortalDetector* GetInstance();

  // Returns true is NetworkPortalDetector service is enabled.
  static bool IsEnabled();

 private:
  friend class NetworkPortalDetectorTest;

  typedef std::string NetworkId;
  typedef base::hash_map<NetworkId, CaptivePortalState> CaptivePortalStateMap;

  enum State {
    // No portal check is running.
    STATE_IDLE,
    // Waiting for portal check.
    STATE_PORTAL_CHECK_PENDING,
    // Portal check is in progress.
    STATE_CHECKING_FOR_PORTAL,
  };

  explicit NetworkPortalDetector(
      const scoped_refptr<net::URLRequestContextGetter>& request_context);

  // Initiates Captive Portal detection after |delay|.
  void DetectCaptivePortal(const base::TimeDelta& delay);

  void DetectCaptivePortalTask();

  // Called when portal check is timed out. Cancels portal check and
  // calls OnPortalDetectionCompleted() with RESULT_NO_RESPONSE as
  // a result.
  void PortalDetectionTimeout();

  void CancelPortalDetection();

  // Called by CaptivePortalDetector when detection completes.
  void OnPortalDetectionCompleted(
      const captive_portal::CaptivePortalDetector::Results& results);

  // Returns true if we're waiting for portal check.
  bool IsPortalCheckPending() const;

  // Returns true if portal check is in progress.
  bool IsCheckingForPortal() const;

  // Stores captive portal state for a |network|.
  void SetCaptivePortalState(const Network* network,
                             CaptivePortalState state);

  // Notifies observers that portal state is changed for a |network|.
  void NotifyPortalStateChanged(const Network* network,
                                CaptivePortalState state);

  // Returns the current TimeTicks.
  base::TimeTicks GetCurrentTimeTicks() const;

  State state() { return state_; }

  // Returns current number of portal detection attempts.
  // Used by unit tests.
  int attempt_count_for_testing() { return attempt_count_; }

  // Sets minimum time between consecutive portal checks for the same
  // network. Used by unit tests.
  void set_min_time_between_attempts_for_testing(const base::TimeDelta& delta) {
    min_time_between_attempts_ = delta;
  }

  // Sets portal detection timeout. Used by unit tests.
  void set_request_timeout_for_testing(const base::TimeDelta& timeout) {
    request_timeout_ = timeout;
  }

  // Returns delay before next portal check. Used by unit tests.
  const base::TimeDelta& next_attempt_delay_for_testing() {
    return next_attempt_delay_;
  }

  // Sets current test time ticks. Used by unit tests.
  void set_time_ticks_for_testing(const base::TimeTicks& time_ticks) {
    time_ticks_for_testing_ = time_ticks;
  }

  // Advances current test time ticks. Used by unit tests.
  void advance_time_ticks_for_testing(const base::TimeDelta& delta) {
    time_ticks_for_testing_ += delta;
  }

  // Unique identifier of the active network.
  std::string active_network_id_;

  // Service path of the active network.
  std::string active_service_path_;

  // Connection state of the active network.
  ConnectionState active_connection_state_;

  State state_;
  CaptivePortalStateMap captive_portal_state_map_;
  ObserverList<Observer> observers_;

  base::CancelableClosure detection_task_;
  base::CancelableClosure detection_timeout_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL test_url_;

  // Detector for checking active network for a portal state.
  scoped_ptr<captive_portal::CaptivePortalDetector> captive_portal_detector_;

  base::WeakPtrFactory<NetworkPortalDetector> weak_ptr_factory_;

  // Number of portal detection attemps for an active network.
  int attempt_count_;

  // Minimum time between consecutive portal checks for the same
  // active network.
  base::TimeDelta min_time_between_attempts_;

  // Start time of portal detection attempt.
  base::TimeTicks attempt_start_time_;

  // Timeout for a portal detection.
  base::TimeDelta request_timeout_;

  // Delay before next portal detection.
  base::TimeDelta next_attempt_delay_;

  // Test time ticks used by unit tests.
  base::TimeTicks time_ticks_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
