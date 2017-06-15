// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/captive_portal/captive_portal_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

class NetworkingConfigTest;

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

class NetworkPortalNotificationController;
class NetworkState;

// This class handles all notifications about network changes from
// NetworkStateHandler and delegates portal detection for the default
// network to CaptivePortalService.
class NetworkPortalDetectorImpl : public NetworkPortalDetector,
                                  public chromeos::NetworkStateHandlerObserver,
                                  public content::NotificationObserver,
                                  public PortalDetectorStrategy::Delegate {
 public:
  static const char kOobeDetectionResultHistogram[];
  static const char kOobeDetectionDurationHistogram[];
  static const char kOobeShillOnlineHistogram[];
  static const char kOobeShillPortalHistogram[];
  static const char kOobeShillOfflineHistogram[];
  static const char kOobePortalToOnlineHistogram[];

  static const char kSessionDetectionResultHistogram[];
  static const char kSessionDetectionDurationHistogram[];
  static const char kSessionShillOnlineHistogram[];
  static const char kSessionShillPortalHistogram[];
  static const char kSessionShillOfflineHistogram[];
  static const char kSessionPortalToOnlineHistogram[];

  NetworkPortalDetectorImpl(
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      bool create_notification_controller);
  ~NetworkPortalDetectorImpl() override;

  // Set the URL to be tested for portal state.
  void set_portal_test_url(const GURL& portal_test_url) {
    portal_test_url_ = portal_test_url;
  }

 private:
  friend class ::NetworkingConfigTest;
  friend class NetworkPortalDetectorImplTest;
  friend class NetworkPortalDetectorImplBrowserTest;

  using CaptivePortalStateMap = base::hash_map<std::string, CaptivePortalState>;

  enum State {
    // No portal check is running.
    STATE_IDLE = 0,
    // Waiting for portal check.
    STATE_PORTAL_CHECK_PENDING,
    // Portal check is in progress.
    STATE_CHECKING_FOR_PORTAL,
    // No portal check when successfully behind portal.
    STATE_BEHIND_PORTAL_IDLE,
  };

  struct DetectionAttemptCompletedReport {
    DetectionAttemptCompletedReport();

    DetectionAttemptCompletedReport(const std::string network_name,
                                    const std::string network_id,
                                    captive_portal::CaptivePortalResult result,
                                    int response_code);

    void Report() const;

    bool Equals(const DetectionAttemptCompletedReport& o) const;

    std::string network_name;
    std::string network_id;
    captive_portal::CaptivePortalResult result;
    int response_code;
  };

  // Starts detection process.
  void StartDetection();

  // Stops whole detection process.
  void StopDetection();

  // Stops and restarts the detection process.
  void RetryDetection();

  // Initiates Captive Portal detection attempt after |delay|.
  void ScheduleAttempt(const base::TimeDelta& delay);

  // Starts detection attempt.
  void StartAttempt();

  // Called when portal check is timed out. Cancels portal check and calls
  // OnPortalDetectionCompleted() with RESULT_NO_RESPONSE as a result.
  void OnAttemptTimeout();

  // Called by CaptivePortalDetector when detection attempt completes.
  void OnAttemptCompleted(
      const captive_portal::CaptivePortalDetector::Results& results);

  // NetworkPortalDetector implementation:
  void AddObserver(Observer* observer) override;
  void AddAndFireObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  CaptivePortalState GetCaptivePortalState(const std::string& guid) override;
  bool IsEnabled() override;
  void Enable(bool start_detection) override;
  bool StartDetectionIfIdle() override;
  void SetStrategy(PortalDetectorStrategy::StrategyId id) override;
  void OnLockScreenRequest() override;

  // NetworkStateHandlerObserver implementation:
  void DefaultNetworkChanged(const NetworkState* network) override;

  // PortalDetectorStrategy::Delegate implementation:
  int NoResponseResultCount() override;
  base::TimeTicks AttemptStartTime() override;
  base::TimeTicks NowTicks() override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Stores captive portal state for a |network| and notifies observers.
  void OnDetectionCompleted(const NetworkState* network,
                            const CaptivePortalState& results);

  // Notifies observers that portal detection is completed for a |network|.
  void NotifyDetectionCompleted(const NetworkState* network,
                                const CaptivePortalState& state);

  State state() const { return state_; }

  bool is_idle() const {
    return state_ == STATE_IDLE;
  }
  bool is_portal_check_pending() const {
    return state_ == STATE_PORTAL_CHECK_PENDING;
  }
  bool is_checking_for_portal() const {
    return state_ == STATE_CHECKING_FOR_PORTAL;
  }
  bool is_behind_portal_idle() const {
    return state_ == STATE_BEHIND_PORTAL_IDLE;
  }

  int same_detection_result_count_for_testing() const {
    return same_detection_result_count_;
  }

  int no_response_result_count_for_testing() const {
    return no_response_result_count_;
  }

  void set_no_response_result_count_for_testing(int count) {
    no_response_result_count_ = count;
  }

  // Returns delay before next portal check. Used by unit tests.
  const base::TimeDelta& next_attempt_delay_for_testing() const {
    return next_attempt_delay_;
  }

  // Returns true if attempt timeout callback isn't fired or
  // cancelled.
  bool AttemptTimeoutIsCancelledForTesting() const;

  // Record detection stats such as detection duration and detection
  // result in UMA.
  void RecordDetectionStats(const NetworkState* network,
                            CaptivePortalStatus status);

  // Resets strategy and all counters used in computations of
  // timeouts.
  void ResetStrategyAndCounters();

  // Sets current test time ticks. Used by unit tests.
  void set_time_ticks_for_testing(const base::TimeTicks& time_ticks) {
    time_ticks_for_testing_ = time_ticks;
  }

  // Advances current test time ticks. Used by unit tests.
  void advance_time_ticks_for_testing(const base::TimeDelta& delta) {
    time_ticks_for_testing_ += delta;
  }

  // Name of the default network.
  std::string default_network_name_;

  // Unique identifier of the default network.
  std::string default_network_id_;

  // Connection state of the default network.
  std::string default_connection_state_;

  State state_ = STATE_IDLE;
  CaptivePortalStateMap portal_state_map_;
  base::ObserverList<Observer> observers_;

  base::CancelableClosure attempt_task_;
  base::CancelableClosure attempt_timeout_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL portal_test_url_;

  // Detector for checking default network for a portal state.
  std::unique_ptr<captive_portal::CaptivePortalDetector>
      captive_portal_detector_;

  // True if the NetworkPortalDetector is enabled.
  bool enabled_ = false;

  // Start time of portal detection.
  base::TimeTicks detection_start_time_;

  // Start time of detection attempt.
  base::TimeTicks attempt_start_time_;

  // Delay before next portal detection.
  base::TimeDelta next_attempt_delay_;

  // Current detection strategy.
  std::unique_ptr<PortalDetectorStrategy> strategy_;

  // Last received result from captive portal detector.
  CaptivePortalStatus last_detection_result_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;

  // Number of detection attempts with same result in a row.
  int same_detection_result_count_ = 0;

  // Number of detection attempts in a row with NO RESPONSE result.
  int no_response_result_count_ = 0;

  // Must be declared before |notification_controller_| as
  // ~NetworkPortalNotificationController() calls
  // NetworkPortalDetectorImpl::RemoveObserver() which uses this.
  SEQUENCE_CHECKER(sequence_checker_);

  // UI notification controller about captive portal state.
  std::unique_ptr<NetworkPortalNotificationController> notification_controller_;

  content::NotificationRegistrar registrar_;

  // Test time ticks used by unit tests.
  base::TimeTicks time_ticks_for_testing_;

  // Contents of a last log message about completed detection attempt.
  DetectionAttemptCompletedReport attempt_completed_report_;

  base::WeakPtrFactory<NetworkPortalDetectorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
