// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"

namespace chromeos {

class NetworkPortalDetectorStub : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorStub();
  virtual ~NetworkPortalDetectorStub();

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

 private:
  friend class UpdateScreenTest;

  typedef std::string NetworkId;
  typedef base::hash_map<NetworkId, CaptivePortalState> CaptivePortalStateMap;

  void SetActiveNetworkForTesting(const Network* network);
  void SetDetectionResultsForTesting(const Network* network,
                                     const CaptivePortalState& state);
  void NotifyObserversForTesting();

  ObserverList<Observer> observers_;
  const Network* active_network_;
  CaptivePortalStateMap portal_state_map_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_
