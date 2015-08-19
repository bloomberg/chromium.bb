// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/external_estimate_provider.h"
#include "net/base/network_change_notifier.h"

namespace chrome {
namespace android {

// Native class that calls Java code exposed by
// ExternalEstimateProviderAndroidHelper.java. Provides network quality
// estimates as provided by Android. Estimates are automatically updated on a
// network change event.
class ExternalEstimateProviderAndroid
    : public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::ExternalEstimateProvider {
 public:
  // Constructs and initializes the underlying provider.
  ExternalEstimateProviderAndroid();

  ~ExternalEstimateProviderAndroid() override;

  // net::ExternalEstimateProvider implementation.
  bool GetRTT(base::TimeDelta* rtt) const override;

  // net::ExternalEstimateProvider implementation.
  bool GetDownstreamThroughputKbps(
      int32_t* downstream_throughput_kbps) const override;

  // net::ExternalEstimateProvider implementation.
  bool GetUpstreamThroughputKbps(
      int32_t* upstream_throughput_kbps) const override;

  // net::ExternalEstimateProvider implementation.
  bool GetTimeSinceLastUpdate(
      base::TimeDelta* time_since_last_update) const override;

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::ExternalEstimateProvider implementation.
  void SetUpdatedEstimateDelegate(
      net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate)
      override;

 private:
  // Places a requests to the provider to update the network quality. Returns
  // true if the request was placed successfully.
  void RequestUpdate() const;

  // Value returned if valid value is unavailable.
  int32_t no_value_ = -1;

  base::android::ScopedJavaGlobalRef<jobject> j_external_estimate_provider_;

  // Notified every time there is an update available from the network quality
  // provider.
  // TODO(tbansal): Add the function that is called by Java side when an update
  // is available.
  net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ExternalEstimateProviderAndroid);
};

bool RegisterExternalEstimateProviderAndroid(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_
