// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_

#include "base/basictypes.h"
#include "net/url_request/url_fetcher.h"

namespace chromeos {

class Network;

// This class handles all notifications about network changes from
// NetworkLibrary and delegates portal detection for the active
// network to CaptivePortalService.
class NetworkPortalDetector {
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

    CaptivePortalStatus status;
    int response_code;
  };

  class Observer {
   public:
    // Called when portal detection is completed for |network|.
    virtual void OnPortalDetectionCompleted(
        const Network* network,
        const CaptivePortalState& state) = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void AddAndFireObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns Captive Portal state for a given |network|.
  virtual CaptivePortalState GetCaptivePortalState(
      const chromeos::Network* network) = 0;

  // Returns true if portal detection is enabled.
  virtual bool IsEnabled() = 0;

  // Enable portal detection. This method is needed because we can't
  // check current network for portal state unless user accepts EULA.
  // If |start_detection| is true and NetworkPortalDetector was
  // disabled previously, portal detection for the active network is
  // initiated by this method.
  virtual void Enable(bool start_detection) = 0;

  // Enables lazy detection mode. In this mode portal detection after
  // first 3 consecutive attemps will be performed once in 30 seconds.
  virtual void EnableLazyDetection() = 0;

  // Dizables lazy detection mode.
  virtual void DisableLazyDetection() = 0;

  // Creates an instance of the NetworkPortalDetector.
  static NetworkPortalDetector* CreateInstance();

  // Gets the instance of the NetworkPortalDetector.
  static NetworkPortalDetector* GetInstance();

  // Returns true is NetworkPortalDetector service is enabled in command line.
  static bool IsEnabledInCommandLine();

 protected:
  NetworkPortalDetector();
  virtual ~NetworkPortalDetector();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
