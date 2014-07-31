// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_quota_manager_bridge_impl.h"

#include <set>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "jni/AwQuotaManagerBridge_jni.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/quota/quota_types.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;
using content::StoragePartition;
using quota::QuotaClient;
using quota::QuotaManager;

namespace android_webview {

namespace {

// This object lives on UI and IO threads. Care need to be taken to make sure
// there are no concurrent accesses to instance variables. Also this object
// is refcounted in the various callbacks, and is destroyed when all callbacks
// are destroyed at the end of DoneOnUIThread.
class GetOriginsTask : public base::RefCountedThreadSafe<GetOriginsTask> {
 public:
  GetOriginsTask(
      const AwQuotaManagerBridgeImpl::GetOriginsCallback& callback,
      QuotaManager* quota_manager);

  void Run();

 private:
  friend class base::RefCountedThreadSafe<GetOriginsTask>;
  ~GetOriginsTask();

  void OnOriginsObtained(const std::set<GURL>& origins,
                         quota::StorageType type);

  void OnUsageAndQuotaObtained(const GURL& origin,
                               quota::QuotaStatusCode status_code,
                               int64 usage,
                               int64 quota);

  void CheckDone();
  void DoneOnUIThread();

  AwQuotaManagerBridgeImpl::GetOriginsCallback ui_callback_;
  scoped_refptr<QuotaManager> quota_manager_;

  std::vector<std::string> origin_;
  std::vector<int64> usage_;
  std::vector<int64> quota_;

  size_t num_callbacks_to_wait_;
  size_t num_callbacks_received_;

  DISALLOW_COPY_AND_ASSIGN(GetOriginsTask);
};

GetOriginsTask::GetOriginsTask(
    const AwQuotaManagerBridgeImpl::GetOriginsCallback& callback,
    QuotaManager* quota_manager)
    : ui_callback_(callback),
      quota_manager_(quota_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GetOriginsTask::~GetOriginsTask() {}

void GetOriginsTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&QuotaManager::GetOriginsModifiedSince,
                 quota_manager_,
                 quota::kStorageTypeTemporary,
                 base::Time()  /* Since beginning of time. */,
                 base::Bind(&GetOriginsTask::OnOriginsObtained, this)));
}

void GetOriginsTask::OnOriginsObtained(
    const std::set<GURL>& origins, quota::StorageType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  num_callbacks_to_wait_ = origins.size();
  num_callbacks_received_ = 0u;

  for (std::set<GURL>::const_iterator origin = origins.begin();
       origin != origins.end();
       ++origin) {
    quota_manager_->GetUsageAndQuota(
        *origin,
        type,
        base::Bind(&GetOriginsTask::OnUsageAndQuotaObtained, this, *origin));
  }

  CheckDone();
}

void GetOriginsTask::OnUsageAndQuotaObtained(const GURL& origin,
                                             quota::QuotaStatusCode status_code,
                                             int64 usage,
                                             int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (status_code == quota::kQuotaStatusOk) {
    origin_.push_back(origin.spec());
    usage_.push_back(usage);
    quota_.push_back(quota);
  }

  ++num_callbacks_received_;
  CheckDone();
}

void GetOriginsTask::CheckDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (num_callbacks_received_ == num_callbacks_to_wait_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GetOriginsTask::DoneOnUIThread, this));
  } else if (num_callbacks_received_ > num_callbacks_to_wait_) {
    NOTREACHED();
  }
}

// This method is to avoid copying the 3 vector arguments into a bound callback.
void GetOriginsTask::DoneOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_callback_.Run(origin_, usage_, quota_);
}

void RunOnUIThread(const base::Closure& task) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    task.Run();
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, task);
  }
}

}  // namespace


// static
jlong GetDefaultNativeAwQuotaManagerBridge(JNIEnv* env, jclass clazz) {
  AwBrowserContext* browser_context =
      AwContentBrowserClient::GetAwBrowserContext();

  AwQuotaManagerBridgeImpl* bridge = static_cast<AwQuotaManagerBridgeImpl*>(
      browser_context->GetQuotaManagerBridge());
  DCHECK(bridge);
  return reinterpret_cast<intptr_t>(bridge);
}

// static
scoped_refptr<AwQuotaManagerBridge> AwQuotaManagerBridgeImpl::Create(
    AwBrowserContext* browser_context) {
  return new AwQuotaManagerBridgeImpl(browser_context);
}

AwQuotaManagerBridgeImpl::AwQuotaManagerBridgeImpl(
    AwBrowserContext* browser_context)
    : weak_factory_(this),
      browser_context_(browser_context) {
}

AwQuotaManagerBridgeImpl::~AwQuotaManagerBridgeImpl() {}

void AwQuotaManagerBridgeImpl::Init(JNIEnv* env, jobject object) {
  java_ref_ = JavaObjectWeakGlobalRef(env, object);
}

StoragePartition* AwQuotaManagerBridgeImpl::GetStoragePartition() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // AndroidWebview does not use per-site storage partitions.
  StoragePartition* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(browser_context_);
  DCHECK(storage_partition);
  return storage_partition;
}

QuotaManager* AwQuotaManagerBridgeImpl::GetQuotaManager() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  QuotaManager* quota_manager = GetStoragePartition()->GetQuotaManager();
  DCHECK(quota_manager);
  return quota_manager;
}

void AwQuotaManagerBridgeImpl::DeleteAllData(JNIEnv* env, jobject object) {
  RunOnUIThread(base::Bind(&AwQuotaManagerBridgeImpl::DeleteAllDataOnUiThread,
                           this));
}

void AwQuotaManagerBridgeImpl::DeleteAllDataOnUiThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetStoragePartition()->ClearData(
      // Clear all web storage data except cookies.
      StoragePartition::REMOVE_DATA_MASK_APPCACHE |
          StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
          StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
          StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
          StoragePartition::REMOVE_DATA_MASK_WEBSQL,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
      GURL(), StoragePartition::OriginMatcherFunction(),
      base::Time(), base::Time::Max(), base::Bind(&base::DoNothing));
}

void AwQuotaManagerBridgeImpl::DeleteOrigin(
    JNIEnv* env, jobject object, jstring origin) {
  base::string16 origin_string(
      base::android::ConvertJavaStringToUTF16(env, origin));
  RunOnUIThread(base::Bind(&AwQuotaManagerBridgeImpl::DeleteOriginOnUiThread,
                           this,
                           origin_string));
}

void AwQuotaManagerBridgeImpl::DeleteOriginOnUiThread(
    const base::string16& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StoragePartition* storage_partition = GetStoragePartition();
  storage_partition->ClearDataForOrigin(
      // All (temporary) QuotaClient types.
      StoragePartition::REMOVE_DATA_MASK_APPCACHE |
          StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
          StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
          StoragePartition::REMOVE_DATA_MASK_WEBSQL,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
      GURL(origin),
      storage_partition->GetURLRequestContext(),
      base::Bind(&base::DoNothing));
}

void AwQuotaManagerBridgeImpl::GetOrigins(
    JNIEnv* env, jobject object, jint callback_id) {
  RunOnUIThread(base::Bind(&AwQuotaManagerBridgeImpl::GetOriginsOnUiThread,
                           this,
                           callback_id));
}

void AwQuotaManagerBridgeImpl::GetOriginsOnUiThread(jint callback_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const GetOriginsCallback ui_callback = base::Bind(
      &AwQuotaManagerBridgeImpl::GetOriginsCallbackImpl,
      weak_factory_.GetWeakPtr(),
      callback_id);

  (new GetOriginsTask(ui_callback, GetQuotaManager()))->Run();
}

void AwQuotaManagerBridgeImpl::GetOriginsCallbackImpl(
    int jcallback_id,
    const std::vector<std::string>& origin,
    const std::vector<int64>& usage,
    const std::vector<int64>& quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwQuotaManagerBridge_onGetOriginsCallback(
      env,
      obj.obj(),
      jcallback_id,
      base::android::ToJavaArrayOfStrings(env, origin).obj(),
      base::android::ToJavaLongArray(env, usage).obj(),
      base::android::ToJavaLongArray(env, quota).obj());
}

namespace {

void OnUsageAndQuotaObtained(
    const AwQuotaManagerBridgeImpl::QuotaUsageCallback& ui_callback,
    quota::QuotaStatusCode status_code,
    int64 usage,
    int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (status_code != quota::kQuotaStatusOk) {
    usage = 0;
    quota = 0;
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(ui_callback, usage, quota));
}

} // namespace

void AwQuotaManagerBridgeImpl::GetUsageAndQuotaForOrigin(
    JNIEnv* env, jobject object,
    jstring origin,
    jint callback_id,
    bool is_quota) {
  base::string16 origin_string(
      base::android::ConvertJavaStringToUTF16(env, origin));
  RunOnUIThread(base::Bind(
      &AwQuotaManagerBridgeImpl::GetUsageAndQuotaForOriginOnUiThread,
      this,
      origin_string,
      callback_id,
      is_quota));
}

void AwQuotaManagerBridgeImpl::GetUsageAndQuotaForOriginOnUiThread(
    const base::string16& origin,
    jint callback_id,
    bool is_quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const QuotaUsageCallback ui_callback = base::Bind(
      &AwQuotaManagerBridgeImpl::QuotaUsageCallbackImpl,
      weak_factory_.GetWeakPtr(),
      callback_id,
      is_quota);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&QuotaManager::GetUsageAndQuota,
                 GetQuotaManager(),
                 GURL(origin),
                 quota::kStorageTypeTemporary,
                 base::Bind(&OnUsageAndQuotaObtained, ui_callback)));
}

void AwQuotaManagerBridgeImpl::QuotaUsageCallbackImpl(
    int jcallback_id, bool is_quota, int64 usage, int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwQuotaManagerBridge_onGetUsageAndQuotaForOriginCallback(
      env, obj.obj(), jcallback_id, is_quota, usage, quota);
}

bool RegisterAwQuotaManagerBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
