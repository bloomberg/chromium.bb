// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/external_estimate_provider.h"

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

  // net::ExternalEstimateProvider implementation:
  bool GetRTT(base::TimeDelta* rtt) const override;
  bool GetDownstreamThroughputKbps(
      int32_t* downstream_throughput_kbps) const override;
  bool GetUpstreamThroughputKbps(
      int32_t* upstream_throughput_kbps) const override;
  bool GetTimeSinceLastUpdate(
      base::TimeDelta* time_since_last_update) const override;
  void SetUpdatedEstimateDelegate(
      net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate)
      override;
  void Update() const override;

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Called by Java when the external estimate provider has an updated value.
  // This may be called on a thread different from |task_runner_|.
  void NotifyExternalEstimateProviderAndroidUpdate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 protected:
  // Notifies the delegate that a new update to external estimate is available.
  // Protected for testing.
  void NotifyUpdatedEstimateAvailable() const;

 private:
  // Value returned if valid value is unavailable.
  int32_t no_value_ = -1;

  base::android::ScopedJavaGlobalRef<jobject> j_external_estimate_provider_;

  // Used to post tasks back to this object's origin thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Notified every time there is an update available from the network quality
  // provider.
  net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ExternalEstimateProviderAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalEstimateProviderAndroid);
};

bool RegisterExternalEstimateProviderAndroid(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_NET_EXTERNAL_ESTIMATE_PROVIDER_ANDROID_H_
