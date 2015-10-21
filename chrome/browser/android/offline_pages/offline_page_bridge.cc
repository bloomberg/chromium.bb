// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "jni/OfflinePageBridge_jni.h"
#include "net/base/filename_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

void SavePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                      const GURL& url,
                      OfflinePageModel::SavePageResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_SavePageCallback_onSavePageDone(
      env, j_callback_obj.obj(), static_cast<int>(result),
      ConvertUTF8ToJavaString(env, url.spec()).obj());
}

void DeletePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                        OfflinePageModel::DeletePageResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_DeletePageCallback_onDeletePageDone(
      env, j_callback_obj.obj(), static_cast<int>(result));
}

void ToJavaOfflinePageList(JNIEnv* env,
                           jobject j_result_obj,
                           const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageBridge_createOfflinePageAndAddToList(
        env, j_result_obj,
        ConvertUTF8ToJavaString(env, offline_page.url.spec()).obj(),
        offline_page.bookmark_id,
        ConvertUTF8ToJavaString(env, offline_page.GetOfflineURL().spec()).obj(),
        offline_page.file_size,
        offline_page.access_count);
  }
}

}  // namespace

static jboolean IsOfflinePagesEnabled(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz) {
  return offline_pages::IsOfflinePagesEnabled();
}

OfflinePageBridge::OfflinePageBridge(JNIEnv* env,
                                     jobject obj,
                                     content::BrowserContext* browser_context)
    : weak_java_ref_(env, obj),
      offline_page_model_(OfflinePageModelFactory::GetForBrowserContext(
          browser_context)),
      browser_context_(browser_context) {
  NotifyIfDoneLoading();
  offline_page_model_->AddObserver(this);
}

void OfflinePageBridge::Destroy(JNIEnv*, jobject) {
  offline_page_model_->RemoveObserver(this);
  delete this;
}

void OfflinePageBridge::OfflinePageModelLoaded(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  NotifyIfDoneLoading();
}

void OfflinePageBridge::OfflinePageModelChanged(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageBridge_offlinePageModelChanged(env, obj.obj());
}

void OfflinePageBridge::OfflinePageDeleted(int64 bookmark_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageBridge_offlinePageDeleted(env, obj.obj(), bookmark_id);
}

void OfflinePageBridge::GetAllPages(JNIEnv* env,
                                    jobject obj,
                                    jobject j_result_obj) {
  DCHECK(offline_page_model_->is_loaded());
  DCHECK(j_result_obj);
  const std::vector<OfflinePageItem>& offline_pages =
      offline_page_model_->GetAllPages();
  ToJavaOfflinePageList(env, j_result_obj, offline_pages);
}

void OfflinePageBridge::GetPagesToCleanUp(JNIEnv* env,
                                          jobject obj,
                                          jobject j_result_obj) {
  DCHECK(offline_page_model_->is_loaded());
  DCHECK(j_result_obj);
  const std::vector<OfflinePageItem>& offline_pages =
      offline_page_model_->GetPagesToCleanUp();
  ToJavaOfflinePageList(env, j_result_obj, offline_pages);
}

ScopedJavaLocalRef<jobject> OfflinePageBridge::GetPageByBookmarkId(
    JNIEnv* env,
    jobject obj,
    jlong bookmark_id) {
  const OfflinePageItem* offline_page =
      offline_page_model_->GetPageByBookmarkId(bookmark_id);
  if (!offline_page)
    return ScopedJavaLocalRef<jobject>();

  return Java_OfflinePageBridge_createOfflinePageItem(
      env, ConvertUTF8ToJavaString(env, offline_page->url.spec()).obj(),
      offline_page->bookmark_id,
      ConvertUTF8ToJavaString(env, offline_page->GetOfflineURL().spec()).obj(),
      offline_page->file_size,
      offline_page->access_count);
}

void OfflinePageBridge::SavePage(JNIEnv* env,
                                 jobject obj,
                                 jobject j_callback_obj,
                                 jobject j_web_contents,
                                 jlong bookmark_id) {
  DCHECK(j_callback_obj);
  DCHECK(j_web_contents);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  GURL url(web_contents->GetLastCommittedURL());

  scoped_ptr<OfflinePageArchiver> archiver(
      new OfflinePageMHTMLArchiver(
          web_contents, GetDownloadsPath(browser_context_)));

  offline_page_model_->SavePage(
      url, bookmark_id, archiver.Pass(),
      base::Bind(&SavePageCallback, j_callback_ref, url));
}

void OfflinePageBridge::MarkPageAccessed(JNIEnv* env,
                                         jobject obj,
                                         jlong bookmark_id) {
  offline_page_model_->MarkPageAccessed(bookmark_id);
}

void OfflinePageBridge::DeletePage(JNIEnv* env,
                                   jobject obj,
                                   jobject j_callback_obj,
                                   jlong bookmark_id) {
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  offline_page_model_->DeletePageByBookmarkId(bookmark_id, base::Bind(
      &DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::DeletePages(JNIEnv* env,
                                    jobject obj,
                                    jobject j_callback_obj,
                                    jlongArray bookmark_ids_array) {
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  std::vector<int64> bookmark_ids;
  base::android::JavaLongArrayToInt64Vector(env, bookmark_ids_array,
                                            &bookmark_ids);

  offline_page_model_->DeletePagesByBookmarkId(
      bookmark_ids,
      base::Bind(&DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::CheckMetadataConsistency(JNIEnv* env, jobject obj) {
  offline_page_model_->CheckForExternalFileDeletion();
}

void OfflinePageBridge::NotifyIfDoneLoading() const {
  if (!offline_page_model_->is_loaded())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageBridge_offlinePageModelLoaded(env, obj.obj());
}

// static
bool OfflinePageBridge::MightBeOfflineURL(const GURL& url) {
  // It has to be a file URL ending with .mhtml extension.
  return url.is_valid() &&
         url.SchemeIsFile() &&
         base::EndsWith(url.spec(),
                        OfflinePageMHTMLArchiver::GetFileNameExtension(),
                        base::CompareCase::INSENSITIVE_ASCII);
}

// static
base::FilePath OfflinePageBridge::GetDownloadsPath(
    content::BrowserContext* browser_context) {
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(browser_context);
  if (!manager) {
    DVLOG(1) << "No download manager available in offline page bridge";
    return base::FilePath();
  }

  return DownloadPrefs::FromDownloadManager(manager)->DownloadPath();
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  return reinterpret_cast<jlong>(new OfflinePageBridge(
      env, obj, ProfileAndroid::FromProfileAndroid(j_profile)));
}

bool RegisterOfflinePageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace offline_pages
