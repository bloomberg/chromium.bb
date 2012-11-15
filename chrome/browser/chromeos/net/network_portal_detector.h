// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
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
      public chromeos::NetworkLibrary::NetworkManagerObserver {
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
    // Portal check is in progress.
    STATE_CHECKING_FOR_PORTAL,
    // Portal check is in progress, but other portal detection request
    // is pending.
    STATE_CHECKING_FOR_PORTAL_NETWORK_CHANGED,
  };

  explicit NetworkPortalDetector(
      const scoped_refptr<net::URLRequestContextGetter>& request_context);

  // Initiates Captive Portal detection. If currently portal detection
  // in in progress, then delays portal detection.
  void DetectCaptivePortal();

  // Called by CaptivePortalDetector when detection completes.
  void OnPortalDetectionCompleted(
      const captive_portal::CaptivePortalDetector::Results& results);

  // Returns true if portal check is in progress.
  bool IsCheckingForPortal() const;

  // Stores captive portal state for a |network|.
  void SetCaptivePortalState(const Network* network,
                             CaptivePortalState state);

  // Notifies observers that portal state is changed for a |network|.
  void NotifyPortalStateChanged(const Network* network,
                                CaptivePortalState state);

  State state() { return state_; }

  std::string active_network_id_;
  State state_;
  CaptivePortalStateMap captive_portal_state_map_;
  ObserverList<Observer> observers_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL test_url_;

  // Detector for checking active network for a portal state.
  scoped_ptr<captive_portal::CaptivePortalDetector> captive_portal_detector_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_H_
