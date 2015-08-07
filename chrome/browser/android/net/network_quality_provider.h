// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NET_NETWORK_QUALITY_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_NET_NETWORK_QUALITY_PROVIDER_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"

// Native class that calls Java code exposed by
// NetworkQualityProviderHelper.java. Provides network quality estimates as
// provided by Android. Estimates are automatically updated on a network change
// event.
class NetworkQualityProvider
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Constructs and initializes the underlying provider.
  NetworkQualityProvider();

  ~NetworkQualityProvider() override;

  // Returns true only if network quality estimate is available.
  bool IsEstimateAvailable();

  // Returns true if the estimated RTT duration is available, and sets |rtt| to
  // the estimate.
  bool GetRTT(base::TimeDelta* rtt);

  // Returns true if the estimated downstream throughput (in Kbps -- Kilobits
  // per second) is available, and sets |downstream_throughput_kbps| to the
  // estimate.
  bool GetDownstreamThroughputKbps(int32_t* downstream_throughput_kbps);

  // Returns true if the estimated upstream throughput (in Kbps -- Kilobits per
  // second) is available, and sets |upstream_throughput_kbps| to the estimate.
  bool GetUpstreamThroughputKbps(int32_t* upstream_throughput_kbps);

  // Returns true if the time since network quality was last updated is
  // available, and sets |time_since_last_update| to that value.
  bool GetTimeSinceLastUpdate(base::TimeDelta* time_since_last_update);

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  // Places a requests to the provider to update the network quality. Returns
  // true if the request was placed successfully.
  void RequestUpdate();

  // Value returned if valid value is unavailable.
  int32_t no_value_ = -1;

  base::android::ScopedJavaGlobalRef<jobject> j_network_quality_provider_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityProvider);
};

bool RegisterNetworkQualityProvider(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_NET_NETWORK_QUALITY_PROVIDER_H_
