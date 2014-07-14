// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace chromeos {

class NetworkPortalDetectorTestImpl : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorTestImpl();
  virtual ~NetworkPortalDetectorTestImpl();

  void SetDefaultNetworkForTesting(const std::string& guid);
  void SetDetectionResultsForTesting(const std::string& guid,
                                     const CaptivePortalState& state);
  void NotifyObserversForTesting();

  // NetworkPortalDetector implementation:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void AddAndFireObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual CaptivePortalState GetCaptivePortalState(
      const std::string& service_path) OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual void Enable(bool start_detection) OVERRIDE;
  virtual bool StartDetectionIfIdle() OVERRIDE;

  virtual void SetStrategy(PortalDetectorStrategy::StrategyId id) OVERRIDE;

  PortalDetectorStrategy::StrategyId strategy_id() const {
    return strategy_id_;
  }

 private:
  typedef std::string NetworkId;
  typedef base::hash_map<NetworkId, CaptivePortalState> CaptivePortalStateMap;

  ObserverList<Observer> observers_;
  scoped_ptr<NetworkState> default_network_;
  CaptivePortalStateMap portal_state_map_;
  PortalDetectorStrategy::StrategyId strategy_id_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorTestImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
