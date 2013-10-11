// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks_bridge.h"

#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "jni/BookmarksBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
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
  if (bookmark_model_->loaded())
    Java_BookmarksBridge_bookmarkModelLoaded(env, obj);
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

static bool IsEditBookmarksEnabled() {
  return ProfileManager::GetLastUsedProfile()->GetPrefs()->GetBoolean(
      prefs::kEditBookmarksEnabled);
}

static jboolean IsEditBookmarksEnabled(JNIEnv* env, jclass clazz) {
  return IsEditBookmarksEnabled();
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

void BookmarksBridge::DeleteBookmark(JNIEnv* env,
                                     jobject obj,
                                     jlong bookmark_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(bookmark_model_->loaded());

  const BookmarkNode* node = bookmark_model_->GetNodeByID(bookmark_id);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }

  const BookmarkNode* parent_node = node->parent();
  bookmark_model_->Remove(parent_node, parent_node->GetIndexOf(node));
}

ScopedJavaLocalRef<jobject> BookmarksBridge::CreateJavaBookmark(
    const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();

  const BookmarkNode* parent = node->parent();
  int64 parent_id = parent ? parent->id() : -1;

  std::string url;
  if (node->is_url())
    url = node->url().spec();

  return Java_BookmarksBridge_create(
      env,
      node->id(),
      ConvertUTF16ToJavaString(env, node->GetTitle()).obj(),
      ConvertUTF8ToJavaString(env, url).obj(),
      node->is_folder(),
      parent_id,
      IsEditable(node));
}

void BookmarksBridge::ExtractBookmarkNodeInformation(
    const BookmarkNode* node,
    jobject j_result_obj) {
  JNIEnv* env = AttachCurrentThread();
  Java_BookmarksBridge_addToList(
      env,
      j_result_obj,
      CreateJavaBookmark(node).obj());
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

bool BookmarksBridge::IsEditable(const BookmarkNode* node) const {
  return node &&
         (node->type() == BookmarkNode::FOLDER ||
          node->type() == BookmarkNode::URL) &&
          IsEditBookmarksEnabled();
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

void BookmarksBridge::BookmarkNodeMoved(BookmarkModel* model,
                                        const BookmarkNode* old_parent,
                                        int old_index,
                                        const BookmarkNode* new_parent,
                                        int new_index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkNodeMoved(
      env,
      obj.obj(),
      CreateJavaBookmark(old_parent).obj(),
      old_index,
      CreateJavaBookmark(new_parent).obj(),
      new_index);
}

void BookmarksBridge::BookmarkNodeAdded(BookmarkModel* model,
                                        const BookmarkNode* parent,
                                        int index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkNodeAdded(
      env,
      obj.obj(),
      CreateJavaBookmark(parent).obj(),
      index);
}

void BookmarksBridge::BookmarkNodeRemoved(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int old_index,
                                          const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkNodeRemoved(
      env,
      obj.obj(),
      CreateJavaBookmark(parent).obj(),
      old_index,
      CreateJavaBookmark(node).obj());
}

void BookmarksBridge::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkNodeChanged(
      env,
      obj.obj(),
      CreateJavaBookmark(node).obj());
}

void BookmarksBridge::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkNodeChildrenReordered(
      env,
      obj.obj(),
      CreateJavaBookmark(node).obj());
}

void BookmarksBridge::ExtensiveBookmarkChangesBeginning(BookmarkModel* model) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_extensiveBookmarkChangesBeginning(env, obj.obj());
}

void BookmarksBridge::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_extensiveBookmarkChangesEnded(env, obj.obj());
}
