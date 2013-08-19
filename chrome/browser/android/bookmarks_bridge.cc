// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/browser_thread.h"
#include "jni/BookmarksBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

BookmarksBridge::BookmarksBridge(JNIEnv* env,
                                 jobject obj,
                                 jobject j_profile)
    : weak_java_ref_(env, obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile);
  // Registers the notifications we are interested.
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded()) {
    Java_BookmarksBridge_bookmarkModelLoaded(env, obj);
  }
}

BookmarksBridge::~BookmarksBridge() {
  bookmark_model_->RemoveObserver(this);
}

void BookmarksBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

// static
bool BookmarksBridge::RegisterBookmarksBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jint Init(JNIEnv* env, jobject obj, jobject j_profile) {
  BookmarksBridge* delegate = new BookmarksBridge(env, obj, j_profile);
  return reinterpret_cast<jint>(delegate);
}

void BookmarksBridge::GetBookmarksForFolder(JNIEnv* env,
                                            jobject obj,
                                            jlong folder_id,
                                            jobject j_callback_obj,
                                            jobject j_result_obj) {
  DCHECK(bookmark_model_->loaded());
  const BookmarkNode* folder = GetFolderNodeFromId(folder_id);
  // Get the folder contents
  for (int i = 0; i < folder->child_count(); ++i) {
    const BookmarkNode* node = folder->GetChild(i);
    ExtractBookmarkNodeInformation(node, j_result_obj);
  }

  Java_BookmarksCallback_onBookmarksAvailable(
      env, j_callback_obj, folder->id(), j_result_obj);
}

void BookmarksBridge::GetCurrentFolderHierarchy(JNIEnv* env,
                                                jobject obj,
                                                jlong folder_id,
                                                jobject j_callback_obj,
                                                jobject j_result_obj) {
  DCHECK(bookmark_model_->loaded());
  const BookmarkNode* folder = GetFolderNodeFromId(folder_id);
  // Get the folder heirarchy
  const BookmarkNode* node = folder;
  while (node) {
    ExtractBookmarkNodeInformation(node, j_result_obj);
    node = node->parent();
  }

  Java_BookmarksCallback_onBookmarksFolderHierarchyAvailable(
      env, j_callback_obj, folder->id(), j_result_obj);
}

void BookmarksBridge::ExtractBookmarkNodeInformation(
    const BookmarkNode* node,
    jobject j_result_obj) {
  JNIEnv* env = AttachCurrentThread();
  const BookmarkNode* parent = node->parent();
  int64 parent_id = -1;
  if (parent)
    parent_id = node->parent()->id();
  std::string url;
  if (node->is_url())
    url = node->url().spec();
  Java_BookmarksBridge_create(
      env, j_result_obj, node->id(),
      ConvertUTF16ToJavaString(env, node->GetTitle()).obj(),
      ConvertUTF8ToJavaString(env, url).obj(),
      node->is_folder(), parent_id);
}

const BookmarkNode* BookmarksBridge::GetFolderNodeFromId(jlong folder_id) {
  const BookmarkNode* folder;
  if (folder_id == -1) {
    folder = bookmark_model_->mobile_node();
  } else {
    folder = bookmark_model_->GetNodeByID(
        static_cast<int64>(folder_id));
  }
  if (!folder)
    folder = bookmark_model_->mobile_node();
  return folder;
}

// ------------- Observer-related methods ------------- //

void BookmarksBridge::BookmarkModelChanged() {
}

void BookmarksBridge::Loaded(BookmarkModel* model, bool ids_reassigned) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkModelLoaded(env, obj.obj());
}

void BookmarksBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkModelDeleted(env, obj.obj());
}
