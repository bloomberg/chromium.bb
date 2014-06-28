// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "net/url_request/url_fetcher.h"

namespace chromeos {

class NetworkState;

// This class handles all notifications about network changes from
// NetworkStateHandler and delegates portal detection for the active
// network to CaptivePortalService.
class CHROMEOS_EXPORT NetworkPortalDetector {
 public:
  enum CaptivePortalStatus {
    CAPTIVE_PORTAL_STATUS_UNKNOWN  = 0,
    CAPTIVE_PORTAL_STATUS_OFFLINE  = 1,
    CAPTIVE_PORTAL_STATUS_ONLINE   = 2,
    CAPTIVE_PORTAL_STATUS_PORTAL   = 3,
    CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED = 4,
    CAPTIVE_PORTAL_STATUS_COUNT
  };

  struct CaptivePortalState {
    CaptivePortalState()
        : status(CAPTIVE_PORTAL_STATUS_UNKNOWN),
          response_code(net::URLFetcher::RESPONSE_CODE_INVALID) {
    }

    bool operator==(const CaptivePortalState& o) const {
      return status == o.status && response_code == o.response_code;
    }

    CaptivePortalStatus status;
    int response_code;
    base::TimeTicks time;
  };

  class Observer {
   public:
    // Called when portal detection is completed for |network|, or
    // when observers add themselves via AddAndFireObserver(). In the
    // second case, |network| is the active network and |state| is a
    // current portal state for the active network, which can be
    // currently in the unknown state, for instance, if portal
    // detection is in process for the active network. Note, that
    // |network| may be NULL.
    virtual void OnPortalDetectionCompleted(
        const NetworkState* network,
        const CaptivePortalState& state) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Adds |observer| to the observers list.
  virtual void AddObserver(Observer* observer) = 0;

  // Adds |observer| to the observers list and immediately calls
  // OnPortalDetectionCompleted() with the active network (which may
  // be NULL) and captive portal state for the active network (which
  // may be unknown, if, for instance, portal detection is in process
  // for the active network).
  //
  // WARNING: don't call this method from the Observer's ctors or
  // dtors, as it implicitly calls OnPortalDetectionCompleted(), which
  // is virtual.
  // TODO (ygorshenin@): find a way to avoid this restriction.
  virtual void AddAndFireObserver(Observer* observer) = 0;

  // Removes |observer| from the observers list.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns Captive Portal state for the network specified by |service_path|.
  virtual CaptivePortalState GetCaptivePortalState(
      const std::string& service_path) = 0;

  // Returns true if portal detection is enabled.
  virtual bool IsEnabled() = 0;

  // Enable portal detection. This method is needed because we can't
  // check current network for portal state unless user accepts EULA.
  // If |start_detection| is true and NetworkPortalDetector was
  // disabled previously, portal detection for the active network is
  // initiated by this method.
  virtual void Enable(bool start_detection) = 0;

  // Restarts portal detection for the default network if currently in
  // the idle state. Returns true if new portal detection attempt was
  // started.
  virtual bool StartDetectionIfIdle() = 0;

  // Sets current strategy according to |id|. If current detection id
  // doesn't equal to |id|, detection is restarted.
  virtual void SetStrategy(PortalDetectorStrategy::StrategyId id) = 0;

  // Initializes network portal detector for testing. The
  // |network_portal_detector| will be owned by the internal pointer
  // and deleted by Shutdown().
  static void InitializeForTesting(
      NetworkPortalDetector* network_portal_detector);

  // Returns |true| if NetworkPortalDetector was Initialized and it is safe to
  // call Get.
  static bool IsInitialized();

  // Deletes the instance of the NetworkPortalDetector.
  static void Shutdown();

  // Gets the instance of the NetworkPortalDetector. Return value should
  // be used carefully in tests, because it can be changed "on the fly"
  // by calls to InitializeForTesting().
  static NetworkPortalDetector* Get();

  // Returns non-localized string representation of |status|.
  static std::string CaptivePortalStatusString(CaptivePortalStatus status);

 protected:
  NetworkPortalDetector() {}
  virtual ~NetworkPortalDetector() {}

  static bool set_for_testing() { return set_for_testing_; }
  static NetworkPortalDetector* network_portal_detector() {
    return network_portal_detector_;
  }
  static void set_network_portal_detector(
      NetworkPortalDetector* network_portal_detector) {
    network_portal_detector_ = network_portal_detector;
  }

 private:
  static bool set_for_testing_;
  static NetworkPortalDetector* network_portal_detector_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetector);
};

class CHROMEOS_EXPORT NetworkPortalDetectorStubImpl
    : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorStubImpl();
  virtual ~NetworkPortalDetectorStubImpl();

 protected:
  // NetworkPortalDetector
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void AddAndFireObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual CaptivePortalState GetCaptivePortalState(
      const std::string& service_path) OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual void Enable(bool start_detection) OVERRIDE;
  virtual bool StartDetectionIfIdle() OVERRIDE;
  virtual void SetStrategy(PortalDetectorStrategy::StrategyId id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorStubImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
