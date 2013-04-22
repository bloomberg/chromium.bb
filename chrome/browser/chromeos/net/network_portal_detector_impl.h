// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// This class handles all notifications about network changes from
// NetworkLibrary and delegates portal detection for the active
// network to CaptivePortalService.
class NetworkPortalDetectorImpl
    : public NetworkPortalDetector,
      public base::NonThreadSafe,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public chromeos::NetworkLibrary::NetworkObserver,
      public content::NotificationObserver {
 public:
  explicit NetworkPortalDetectorImpl(
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  virtual ~NetworkPortalDetectorImpl();

  // NetworkPortalDetector implementation:
  virtual void Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void AddAndFireObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual CaptivePortalState GetCaptivePortalState(
      const chromeos::Network* network) OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual void Enable(bool start_detection) OVERRIDE;
  virtual void EnableLazyDetection() OVERRIDE;
  virtual void DisableLazyDetection() OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros) OVERRIDE;

  // NetworkLibrary::NetworkObserver implementation:
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                const chromeos::Network* network) OVERRIDE;

 private:
  friend class NetworkPortalDetectorImplTest;

  typedef std::string NetworkId;
  typedef base::hash_map<NetworkId, CaptivePortalState> CaptivePortalStateMap;
  enum State {
    // No portal check is running.
    STATE_IDLE = 0,
    // Waiting for portal check.
    STATE_PORTAL_CHECK_PENDING,
    // Portal check is in progress.
    STATE_CHECKING_FOR_PORTAL,
  };

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

  // Tries to perform portal detection in "lazy" mode. Does nothing in
  // the case of already pending/processing detection request.
  void TryLazyDetection();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if we're waiting for portal check.
  bool IsPortalCheckPending() const;

  // Returns true if portal check is in progress.
  bool IsCheckingForPortal() const;

  // Stores captive portal state for a |network|.
  void SetCaptivePortalState(const Network* network,
                             const CaptivePortalState& results);

  // Notifies observers that portal detection is completed for a |network|.
  void NotifyPortalDetectionCompleted(const Network* network,
                                      const CaptivePortalState& state);

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

  // Sets default interval between consecutive portal checks for a
  // network in portal state. Used by unit tests.
  void set_lazy_check_interval_for_testing(const base::TimeDelta& delta) {
    lazy_check_interval_ = delta;
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

  // Returns true if detection timeout callback isn't fired or
  // cancelled.
  bool DetectionTimeoutIsCancelledForTesting() const;

  // Unique identifier of the active network.
  std::string active_network_id_;

  // Service path of the active network.
  std::string active_service_path_;

  // Connection state of the active network.
  ConnectionState active_connection_state_;

  State state_;
  CaptivePortalStateMap portal_state_map_;
  ObserverList<Observer> observers_;

  base::CancelableClosure detection_task_;
  base::CancelableClosure detection_timeout_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL test_url_;

  // Detector for checking active network for a portal state.
  scoped_ptr<captive_portal::CaptivePortalDetector> captive_portal_detector_;

  // True if the NetworkPortalDetector is enabled.
  bool enabled_;

  base::WeakPtrFactory<NetworkPortalDetectorImpl> weak_ptr_factory_;

  // Number of portal detection attemps for an active network.
  int attempt_count_;

  // True if lazy detection is enabled.
  bool lazy_detection_enabled_;

  // Time between consecutive portal checks for a network in lazy
  // mode.
  base::TimeDelta lazy_check_interval_;

  // Minimum time between consecutive portal checks for the same
  // active network.
  base::TimeDelta min_time_between_attempts_;

  // Start time of portal detection.
  base::TimeTicks detection_start_time_;

  // Start time of portal detection attempt.
  base::TimeTicks attempt_start_time_;

  // Timeout for a portal detection.
  base::TimeDelta request_timeout_;

  // Delay before next portal detection.
  base::TimeDelta next_attempt_delay_;

  // Test time ticks used by unit tests.
  base::TimeTicks time_ticks_for_testing_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
