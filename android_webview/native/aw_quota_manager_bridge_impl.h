// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_QUOTA_MANAGER_BRIDGE_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_QUOTA_MANAGER_BRIDGE_IMPL_H_

#include <jni.h>
#include <string>
#include <vector>

#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

class GURL;

namespace content {
class StoragePartition;
}

namespace storage {
class QuotaManager;
}  // namespace storage

namespace android_webview {

class AwBrowserContext;

class AwQuotaManagerBridgeImpl : public AwQuotaManagerBridge {
 public:
  static scoped_refptr<AwQuotaManagerBridge> Create(
      AwBrowserContext* browser_context);

  // Called by Java.
  void Init(JNIEnv* env, jobject object);
  void DeleteAllData(JNIEnv* env, jobject object);
  void DeleteOrigin(JNIEnv* env, jobject object, jstring origin);
  void GetOrigins(JNIEnv* env, jobject object, jint callback_id);
  void GetUsageAndQuotaForOrigin(JNIEnv* env,
                                 jobject object,
                                 jstring origin,
                                 jint callback_id,
                                 bool is_quota);

  typedef base::Callback<
      void(const std::vector<std::string>& /* origin */,
           const std::vector<int64>& /* usage */,
           const std::vector<int64>& /* quota */)> GetOriginsCallback;
  typedef base::Callback<void(int64 /* usage */,
                              int64 /* quota */)> QuotaUsageCallback;

 private:
  explicit AwQuotaManagerBridgeImpl(AwBrowserContext* browser_context);
  virtual ~AwQuotaManagerBridgeImpl();

  content::StoragePartition* GetStoragePartition() const;

  storage::QuotaManager* GetQuotaManager() const;

  void DeleteAllDataOnUiThread();
  void DeleteOriginOnUiThread(const base::string16& origin);
  void GetOriginsOnUiThread(jint callback_id);
  void GetUsageAndQuotaForOriginOnUiThread(const base::string16& origin,
                                           jint callback_id,
                                           bool is_quota);

  void GetOriginsCallbackImpl(
      int jcallback_id,
      const std::vector<std::string>& origin,
      const std::vector<int64>& usage,
      const std::vector<int64>& quota);
  void QuotaUsageCallbackImpl(
      int jcallback_id, bool is_quota, int64 usage, int64 quota);

  base::WeakPtrFactory<AwQuotaManagerBridgeImpl> weak_factory_;
  AwBrowserContext* browser_context_;
  JavaObjectWeakGlobalRef java_ref_;

  DISALLOW_COPY_AND_ASSIGN(AwQuotaManagerBridgeImpl);
};

bool RegisterAwQuotaManagerBridge(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_QUOTA_MANAGER_BRIDGE_IMPL_H_
