// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/enhanced_bookmarks/enhanced_bookmarks_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/enhanced_bookmarks/chrome_bookmark_server_cluster_service.h"
#include "chrome/browser/enhanced_bookmarks/chrome_bookmark_server_cluster_service_factory.h"
#include "chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/chrome_version_info.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "jni/EnhancedBookmarksBridge_jni.h"

using base::android::AttachCurrentThread;
using bookmarks::BookmarkType;

namespace enhanced_bookmarks {
namespace android {

EnhancedBookmarksBridge::EnhancedBookmarksBridge(JNIEnv* env,
    jobject obj,
    Profile* profile) : weak_java_ref_(env, obj) {
  profile_ = profile;
  enhanced_bookmark_model_ =
      EnhancedBookmarkModelFactory::GetForBrowserContext(profile_);
  enhanced_bookmark_model_->SetVersionSuffix(chrome::VersionInfo().OSType());
  cluster_service_ =
      ChromeBookmarkServerClusterServiceFactory::GetForBrowserContext(profile_);
  cluster_service_->AddObserver(this);
}

EnhancedBookmarksBridge::~EnhancedBookmarksBridge() {
  cluster_service_->RemoveObserver(this);
}

void EnhancedBookmarksBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

ScopedJavaLocalRef<jstring> EnhancedBookmarksBridge::GetBookmarkDescription(
    JNIEnv* env, jobject obj, jlong id, jint type) {
  DCHECK(enhanced_bookmark_model_->loaded());
  DCHECK_EQ(BookmarkType::BOOKMARK_TYPE_NORMAL, type);

  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(), static_cast<int64>(id));

  return node ? base::android::ConvertUTF8ToJavaString(
                    env, enhanced_bookmark_model_->GetDescription(node))
              : ScopedJavaLocalRef<jstring>();
}

void EnhancedBookmarksBridge::SetBookmarkDescription(JNIEnv* env,
                                                     jobject obj,
                                                     jlong id,
                                                     jint type,
                                                     jstring description) {
  DCHECK(enhanced_bookmark_model_->loaded());
  DCHECK_EQ(type, BookmarkType::BOOKMARK_TYPE_NORMAL);

  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(), static_cast<int64>(id));

  enhanced_bookmark_model_->SetDescription(
      node, base::android::ConvertJavaStringToUTF8(env, description));
}

void EnhancedBookmarksBridge::GetBookmarksForFilter(JNIEnv* env,
                                                    jobject obj,
                                                    jstring j_filter,
                                                    jobject j_result_obj) {
  DCHECK(enhanced_bookmark_model_->loaded());
  const std::string title =
      base::android::ConvertJavaStringToUTF8(env, j_filter);
  const std::vector<const BookmarkNode*> bookmarks =
      cluster_service_->BookmarksForClusterNamed(title);
  for (const BookmarkNode* node : bookmarks) {
    Java_EnhancedBookmarksBridge_addToBookmarkIdList(
        env, j_result_obj, node->id(), node->type());
  }
}

ScopedJavaLocalRef<jobjectArray> EnhancedBookmarksBridge::GetFilters(
    JNIEnv* env,
    jobject obj) {
  DCHECK(enhanced_bookmark_model_->loaded());
  const std::vector<std::string> filters =
      cluster_service_->GetClusters();
  return base::android::ToJavaArrayOfStrings(env, filters);
}

void EnhancedBookmarksBridge::OnChange(BookmarkServerService* service) {
  DCHECK(enhanced_bookmark_model_->loaded());
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_EnhancedBookmarksBridge_onFiltersChanged(env, obj.obj());
}

static jlong Init(JNIEnv* env, jobject obj, jobject j_profile) {
  return reinterpret_cast<jlong>(new EnhancedBookmarksBridge(
        env, obj, ProfileAndroid::FromProfileAndroid(j_profile)));
}

bool RegisterEnhancedBookmarksBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace enhanced_bookmarks
