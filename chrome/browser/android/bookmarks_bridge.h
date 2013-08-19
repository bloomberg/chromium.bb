// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"

class Profile;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
class BookmarksBridge : public BaseBookmarkModelObserver {
 public:
  BookmarksBridge(JNIEnv* env, jobject obj, jobject j_profile);
  void Destroy(JNIEnv*, jobject);

  static bool RegisterBookmarksBridge(JNIEnv* env);

  void GetBookmarksForFolder(JNIEnv* env,
                             jobject obj,
                             jlong folder_id,
                             jobject j_callback_obj,
                             jobject j_result_obj);

  void GetCurrentFolderHierarchy(JNIEnv* env,
                                 jobject obj,
                                 jlong folder_id,
                                 jobject j_callback_obj,
                                 jobject j_result_obj);

 private:
  virtual ~BookmarksBridge();

  void ExtractBookmarkNodeInformation(
      const BookmarkNode* node,
      jobject j_result_obj);
  const BookmarkNode* GetFolderNodeFromId(jlong folder_id);

  // Override BaseBookmarkModelObserver.
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  JavaObjectWeakGlobalRef weak_java_ref_;
  BookmarkModel* bookmark_model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarksBridge);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_BRIDGE_H_
