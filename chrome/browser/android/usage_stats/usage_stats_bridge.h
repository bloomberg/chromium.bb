// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"

namespace usage_stats {

class UsageStatsDatabase;

/* Native counterpart of UsageStatsBridge.java. Holds non-owning pointers to
 * native implementation to which operations are delegated. This bridge is
 * instantiated, owned, and destroyed from Java.
 */
class UsageStatsBridge {
 public:
  explicit UsageStatsBridge(
      std::unique_ptr<UsageStatsDatabase> usage_stats_database);
  ~UsageStatsBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void GetAllEvents(JNIEnv* j_env,
                    const base::android::JavaRef<jobject>& j_this,
                    const base::android::JavaRef<jobject>& j_callback);

  void QueryEventsInRange(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jlong j_start,
                          const jlong j_end,
                          const base::android::JavaRef<jobject>& j_callback);

  void AddEvents(JNIEnv* j_env,
                 const base::android::JavaRef<jobject>& j_this,
                 const base::android::JavaRef<jobjectArray>& j_events,
                 const base::android::JavaRef<jobject>& j_callback);

  void DeleteAllEvents(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const base::android::JavaRef<jobject>& j_callback);

  void DeleteEventsInRange(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jlong j_start,
                           const jlong j_end,
                           const base::android::JavaRef<jobject>& j_callback);

  void DeleteEventsWithMatchingDomains(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jobjectArray>& j_domains,
      const base::android::JavaRef<jobject>& j_callback);

  void GetAllSuspensions(JNIEnv* j_env,
                         const base::android::JavaRef<jobject>& j_this,
                         const base::android::JavaRef<jobject>& j_callback);

  void SetSuspensions(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const base::android::JavaRef<jobjectArray>& j_domains,
                      const base::android::JavaRef<jobject>& j_callback);

  void GetAllTokenMappings(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const base::android::JavaRef<jobject>& j_callback);

  void SetTokenMappings(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobject>& j_mappings,
                        const base::android::JavaRef<jobject>& j_callback);

 private:
  std::unique_ptr<UsageStatsDatabase> usage_stats_database_;

  base::WeakPtrFactory<UsageStatsBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageStatsBridge);
};

}  // namespace usage_stats

#endif  // CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_
