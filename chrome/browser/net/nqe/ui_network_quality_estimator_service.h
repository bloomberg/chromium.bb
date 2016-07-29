// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
#define CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/nqe/effective_connection_type.h"

// UI service to determine the current EffectiveConnectionType.
class UINetworkQualityEstimatorService : public KeyedService {
 public:
  UINetworkQualityEstimatorService();
  ~UINetworkQualityEstimatorService() override;

  // The current EffectiveConnectionType.
  net::EffectiveConnectionType GetEffectiveConnectionType() const;

  // Tests can manually set EffectiveConnectionType, but browser tests should
  // expect that the EffectiveConnectionType could change.
  void SetEffectiveConnectionTypeForTesting(net::EffectiveConnectionType type);

 private:
  class IONetworkQualityObserver;

  // KeyedService implementation:
  void Shutdown() override;

  // Called when the EffectiveConnectionType has updated to |type|.
  // NetworkQualityEstimator::EffectiveConnectionType is an estimate of the
  // quality of the network that may differ from the actual network type
  // reported by NetworkchangeNotifier::GetConnectionType.
  void EffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // The current EffectiveConnectionType.
  net::EffectiveConnectionType type_;

  // IO thread based observer that is owned by this service. Created on the UI
  // thread, but used and deleted on the IO thread.
  IONetworkQualityObserver* io_observer_;

  base::WeakPtrFactory<UINetworkQualityEstimatorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UINetworkQualityEstimatorService);
};

#endif  // CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
