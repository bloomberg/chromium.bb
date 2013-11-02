// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARKS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARKS_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/android/bookmarks/managed_bookmarks_shim.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"

class Profile;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
class BookmarksBridge : public BaseBookmarkModelObserver,
                        public ManagedBookmarksShim::Observer {
 public:
  BookmarksBridge(JNIEnv* env, jobject obj, jobject j_profile);
  void Destroy(JNIEnv*, jobject);

  static bool RegisterBookmarksBridge(JNIEnv* env);

  void GetBookmarksForFolder(JNIEnv* env,
                             jobject obj,
                             jobject j_folder_id_obj,
                             jobject j_callback_obj,
                             jobject j_result_obj);

  void GetCurrentFolderHierarchy(JNIEnv* env,
                                 jobject obj,
                                 jobject j_folder_id_obj,
                                 jobject j_callback_obj,
                                 jobject j_result_obj);

  void DeleteBookmark(JNIEnv* env,
                      jobject obj,
                      jobject j_bookmark_id_obj);

 private:
  virtual ~BookmarksBridge();

  base::android::ScopedJavaLocalRef<jobject> CreateJavaBookmark(
      const BookmarkNode* node);
  void ExtractBookmarkNodeInformation(
      const BookmarkNode* node,
      jobject j_result_obj);
  const BookmarkNode* GetNodeByID(long node_id, int type);
  const BookmarkNode* GetFolderWithFallback(long folder_id, int type);
  // Returns true if |node| can be modified by the user.
  bool IsEditable(const BookmarkNode* node) const;
  const BookmarkNode* GetParentNode(const BookmarkNode* node);
  int GetBookmarkType(const BookmarkNode* node);

  // Override BaseBookmarkModelObserver.
  // Called when there are changes to the bookmark model that don't trigger
  // any of the other callback methods. For example, this is called when
  // managed or partner bookmarks change.
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE;

  // Override ManagedBookmarksShim::Observer
  virtual void OnManagedBookmarksChanged() OVERRIDE;

  JavaObjectWeakGlobalRef weak_java_ref_;
  BookmarkModel* bookmark_model_;  // weak

  scoped_ptr<ManagedBookmarksShim> managed_bookmarks_shim_;

  DISALLOW_COPY_AND_ASSIGN(BookmarksBridge);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARKS_BRIDGE_H_
