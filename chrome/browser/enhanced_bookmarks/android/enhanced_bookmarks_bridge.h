// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/bookmark_server_search_service.h"
#include "components/enhanced_bookmarks/bookmark_server_service.h"

namespace enhanced_bookmarks {

class BookmarkServerClusterService;
class BookmarkImageServiceAndroid;

namespace android {

class EnhancedBookmarksBridge : public BookmarkServerServiceObserver {
 public:
  EnhancedBookmarksBridge(JNIEnv* env, jobject obj, Profile* profile);
  ~EnhancedBookmarksBridge() override;
  void Destroy(JNIEnv*, jobject);

  void SalientImageForUrl(JNIEnv* env,
                          jobject obj,
                          jstring j_url,
                          jobject j_callback);

  void FetchImageForTab(JNIEnv* env, jobject obj, jobject j_web_contents);

  base::android::ScopedJavaLocalRef<jstring> GetBookmarkDescription(
      JNIEnv* env,
      jobject obj,
      jlong id,
      jint type);
  void SetBookmarkDescription(JNIEnv* env,
                              jobject obj,
                              jlong id,
                              jint type,
                              jstring description);

  base::android::ScopedJavaLocalRef<jobjectArray> GetFiltersForBookmark(
      JNIEnv* env,
      jobject obj,
      jlong id,
      jint type);
  void GetBookmarksForFilter(JNIEnv* env,
                             jobject obj,
                             jstring filter,
                             jobject j_result_obj);
  base::android::ScopedJavaLocalRef<jobjectArray> GetFilters(JNIEnv* env,
                                                             jobject obj);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(JNIEnv* env,
                                                       jobject obj,
                                                       jobject j_parent_id_obj,
                                                       jint index,
                                                       jstring j_title);

  void MoveBookmark(JNIEnv* env,
                    jobject obj,
                    jobject j_bookmark_id_obj,
                    jobject j_parent_id_obj);

  base::android::ScopedJavaLocalRef<jobject> AddBookmark(
      JNIEnv* env,
      jobject obj,
      jobject j_parent_id_obj,
      jint index,
      jstring j_title,
      jstring j_url);
  void SendSearchRequest(JNIEnv* env, jobject obj, jstring j_query);

  base::android::ScopedJavaLocalRef<jobject> GetSearchResults(JNIEnv* env,
                                                              jobject obj,
                                                              jstring j_query);

  // BookmarkServerServiceObserver
  // Called on changes to cluster data or search results are returned.
  void OnChange(BookmarkServerService* service) override;

 private:
  bool IsEditable(const bookmarks::BookmarkNode* node) const;

  JavaObjectWeakGlobalRef weak_java_ref_;
  EnhancedBookmarkModel* enhanced_bookmark_model_;         // weak
  BookmarkServerClusterService* cluster_service_;          // weak
  BookmarkImageServiceAndroid* bookmark_image_service_;    // weak
  scoped_ptr<BookmarkServerSearchService> search_service_;
  Profile* profile_;                       // weak
  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarksBridge);
};

bool RegisterEnhancedBookmarksBridge(JNIEnv* env);

}  // namespace android
}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
