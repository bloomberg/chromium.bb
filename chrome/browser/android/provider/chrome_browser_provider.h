// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/history_service_observer.h"

class AndroidHistoryProviderService;
class FaviconService;
class Profile;

namespace history {
class TopSites;
}

namespace sql {
class Statement;
}

// This class implements the native methods of ChromeBrowserProvider.java
class ChromeBrowserProvider : public bookmarks::BaseBookmarkModelObserver,
                              public history::HistoryServiceObserver {
 public:
  ChromeBrowserProvider(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv*, jobject);

  // JNI registration.
  static bool RegisterChromeBrowserProvider(JNIEnv* env);

  // Adds either a new bookmark or bookmark folder based on |is_folder|.  The
  // bookmark is added to the beginning of the specified parent and if the
  // parent ID is not valid (i.e. < 0) then it will be added to the bookmark
  // bar.
  jlong AddBookmark(JNIEnv* env, jobject,
                    jstring jurl,
                    jstring jtitle,
                    jboolean is_folder,
                    jlong parent_id);

  // Removes a bookmark (or folder) with the specified ID.
  jint RemoveBookmark(JNIEnv*, jobject, jlong id);

  // Updates a bookmark (or folder) with the the new title and new URL.
  // The |url| field will be ignored if the bookmark node is a folder.
  // If a valid |parent_id| (>= 0) different from the currently specified
  // parent is given, the node will be moved to that folder as the first
  // child.
  jint UpdateBookmark(JNIEnv* env,
                      jobject,
                      jlong id,
                      jstring url,
                      jstring title,
                      jlong parent_id);

  // The below are methods to support Android public API.
  // Bookmark and history APIs. -----------------------------------------------
  jlong AddBookmarkFromAPI(JNIEnv* env,
                           jobject obj,
                           jstring url,
                           jobject created,
                           jobject isBookmark,
                           jobject date,
                           jbyteArray favicon,
                           jstring title,
                           jobject visits,
                           jlong parent_id);

  base::android::ScopedJavaLocalRef<jobject> QueryBookmarkFromAPI(
      JNIEnv* env,
      jobject obj,
      jobjectArray projection,
      jstring selections,
      jobjectArray selection_args,
      jstring sort_order);

  jint UpdateBookmarkFromAPI(JNIEnv* env,
                             jobject obj,
                             jstring url,
                             jobject created,
                             jobject isBookmark,
                             jobject date,
                             jbyteArray favicon,
                             jstring title,
                             jobject visits,
                             jlong parent_id,
                             jstring selections,
                             jobjectArray selection_args);

  jint RemoveBookmarkFromAPI(JNIEnv* env,
                             jobject obj,
                             jstring selections,
                             jobjectArray selection_args);

  jint RemoveHistoryFromAPI(JNIEnv* env,
                            jobject obj,
                            jstring selections,
                            jobjectArray selection_args);

  jlong AddSearchTermFromAPI(JNIEnv* env,
                             jobject obj,
                             jstring search_term,
                             jobject date);

  base::android::ScopedJavaLocalRef<jobject> QuerySearchTermFromAPI(
      JNIEnv* env,
      jobject obj,
      jobjectArray projection,
      jstring selections,
      jobjectArray selection_args,
      jstring sort_order);

  jint RemoveSearchTermFromAPI(JNIEnv* env,
                               jobject obj,
                               jstring selections,
                               jobjectArray selection_args);

  jint UpdateSearchTermFromAPI(JNIEnv* env,
                               jobject obj,
                               jstring search_term,
                               jobject date,
                               jstring selections,
                               jobjectArray selection_args);

  // Custom provider API methods. ---------------------------------------------
  jlong CreateBookmarksFolderOnce(JNIEnv* env,
                                  jobject obj,
                                  jstring title,
                                  jlong parent_id);

  void RemoveAllUserBookmarks(JNIEnv* env, jobject obj);

  base::android::ScopedJavaLocalRef<jobject> GetEditableBookmarkFolders(
      JNIEnv* env,
      jobject obj);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkNode(
      JNIEnv* env,
      jobject obj,
      jlong id,
      jboolean get_parent,
      jboolean get_children);

  base::android::ScopedJavaLocalRef<jobject> GetMobileBookmarksFolder(
      JNIEnv* env,
      jobject obj);

  jboolean IsBookmarkInMobileBookmarksBranch(JNIEnv* env,
                                             jobject obj,
                                             jlong id);

  jboolean BookmarkNodeExists(JNIEnv* env,
                              jobject obj,
                              jlong id);

  base::android::ScopedJavaLocalRef<jbyteArray> GetFaviconOrTouchIcon(
      JNIEnv* env,
      jobject obj,
      jstring url);

  base::android::ScopedJavaLocalRef<jbyteArray> GetThumbnail(JNIEnv* env,
                                                             jobject obj,
                                                             jstring url);

 private:
  ~ChromeBrowserProvider() override;

  // Override bookmarks::BaseBookmarkModelObserver.
  void BookmarkModelChanged() override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  // Deals with updates to the history service.
  void OnHistoryChanged();

  // Override history::HistoryServiceObserver.
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;
  void OnKeywordSearchTermUpdated(history::HistoryService* history_service,
                                  const history::URLRow& row,
                                  history::KeywordID keyword_id,
                                  const base::string16& term) override;
  void OnKeywordSearchTermDeleted(history::HistoryService* history_service,
                                  history::URLID url_id) override;

  JavaObjectWeakGlobalRef weak_java_provider_;

  // Profile must outlive this object.
  //
  // BookmarkModel, HistoryService and history::TopSites lifetime is bound to
  // the lifetime of Profile, they are safe to use as long as the Profile is
  // alive.
  Profile* profile_;
  bookmarks::BookmarkModel* bookmark_model_;
  scoped_refptr<history::TopSites> top_sites_;
  FaviconService* favicon_service_;

  scoped_ptr<AndroidHistoryProviderService> service_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  bool handling_extensive_changes_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProvider);
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_
