// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"

namespace chromeos {

class NetworkPortalDetectorStub : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorStub();
  virtual ~NetworkPortalDetectorStub();

  void SetDefaultNetworkPathForTesting(const std::string& service_path);
  void SetDetectionResultsForTesting(const std::string& service_path,
                                     const CaptivePortalState& state);
  void NotifyObserversForTesting();

  // NetworkPortalDetector implementation:
  virtual void Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void AddAndFireObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual CaptivePortalState GetCaptivePortalState(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual void Enable(bool start_detection) OVERRIDE;
  virtual bool StartDetectionIfIdle() OVERRIDE;
  virtual void EnableLazyDetection() OVERRIDE;
  virtual void DisableLazyDetection() OVERRIDE;

 private:
  typedef std::string NetworkId;
  typedef base::hash_map<NetworkId, CaptivePortalState> CaptivePortalStateMap;


  ObserverList<Observer> observers_;
  scoped_ptr<NetworkState> default_network_;
  CaptivePortalStateMap portal_state_map_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_STUB_H_
