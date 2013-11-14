// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"

#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
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

// Should mirror constants in BookmarkBridge.java
static const int kBookmarkTypeNormal = 0;
static const int kBookmarkTypeManaged = 1;
static const int kBookmarkTypePartner = 2;

BookmarksBridge::BookmarksBridge(JNIEnv* env,
                                 jobject obj,
                                 jobject j_profile)
    : weak_java_ref_(env, obj),
      bookmark_model_(NULL),
      partner_bookmarks_shim_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile);

  // Registers the notifications we are interested.
  bookmark_model_->AddObserver(this);

  // Create the partner Bookmarks shim as early as possible (but don't attach).
  partner_bookmarks_shim_ = PartnerBookmarksShim::BuildForBrowserContext(
      chrome::GetBrowserContextRedirectedInIncognito(profile));
  partner_bookmarks_shim_->AddObserver(this);

  managed_bookmarks_shim_.reset(new ManagedBookmarksShim(profile->GetPrefs()));
  managed_bookmarks_shim_->AddObserver(this);

  NotifyIfDoneLoading();

  // Since a sync or import could have started before this class is
  // initialized, we need to make sure that our initial state is
  // up to date.
  if (bookmark_model_->IsDoingExtensiveChanges())
    ExtensiveBookmarkChangesBeginning(bookmark_model_);
}

BookmarksBridge::~BookmarksBridge() {
  bookmark_model_->RemoveObserver(this);
  if (partner_bookmarks_shim_)
    partner_bookmarks_shim_->RemoveObserver(this);
  if (managed_bookmarks_shim_)
    managed_bookmarks_shim_->RemoveObserver(this);
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
                                            jobject j_folder_id_obj,
                                            jobject j_callback_obj,
                                            jobject j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = Java_BookmarkId_getId(env, j_folder_id_obj);
  int type = Java_BookmarkId_getType(env, j_folder_id_obj);
  const BookmarkNode* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // If this is the Mobile bookmarks folder then add the "Managed bookmarks"
  // folder first, so that it's the first entry.
  if (folder == bookmark_model_->mobile_node() &&
      managed_bookmarks_shim_->HasManagedBookmarks()) {
    ExtractBookmarkNodeInformation(
        managed_bookmarks_shim_->GetManagedBookmarksRoot(),
        j_result_obj);
  }
  // Get the folder contents
  for (int i = 0; i < folder->child_count(); ++i) {
    const BookmarkNode* node = folder->GetChild(i);
    ExtractBookmarkNodeInformation(node, j_result_obj);
  }

  if (folder == bookmark_model_->mobile_node() &&
      partner_bookmarks_shim_->HasPartnerBookmarks()) {
    ExtractBookmarkNodeInformation(
        partner_bookmarks_shim_->GetPartnerBookmarksRoot(),
        j_result_obj);
  }

  Java_BookmarksCallback_onBookmarksAvailable(
      env, j_callback_obj, j_folder_id_obj, j_result_obj);
}

void BookmarksBridge::GetCurrentFolderHierarchy(JNIEnv* env,
                                                jobject obj,
                                                jobject j_folder_id_obj,
                                                jobject j_callback_obj,
                                                jobject j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = Java_BookmarkId_getId(env, j_folder_id_obj);
  int type = Java_BookmarkId_getType(env, j_folder_id_obj);
  const BookmarkNode* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Get the folder heirarchy
  const BookmarkNode* node = folder;
  while (node) {
    ExtractBookmarkNodeInformation(node, j_result_obj);
    node = GetParentNode(node);
  }

  Java_BookmarksCallback_onBookmarksFolderHierarchyAvailable(
      env, j_callback_obj, j_folder_id_obj, j_result_obj);
}

void BookmarksBridge::DeleteBookmark(JNIEnv* env,
                                     jobject obj,
                                     jobject j_bookmark_id_obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(IsLoaded());

  long bookmark_id = Java_BookmarkId_getId(env, j_bookmark_id_obj);
  int type = Java_BookmarkId_getType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }

  if (partner_bookmarks_shim_->IsPartnerBookmark(node)) {
    partner_bookmarks_shim_->RemoveBookmark(node);
  } else {
    const BookmarkNode* parent_node = GetParentNode(node);
    bookmark_model_->Remove(parent_node, parent_node->GetIndexOf(node));
  }
}

ScopedJavaLocalRef<jobject> BookmarksBridge::CreateJavaBookmark(
    const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();

  const BookmarkNode* parent = GetParentNode(node);
  int64 parent_id = parent ? parent->id() : -1;

  std::string url;
  if (node->is_url())
    url = node->url().spec();

  return Java_BookmarksBridge_createBookmarkItem(
      env,
      node->id(),
      GetBookmarkType(node),
      ConvertUTF16ToJavaString(env, GetTitle(node)).obj(),
      ConvertUTF8ToJavaString(env, url).obj(),
      node->is_folder(),
      parent_id,
      GetBookmarkType(parent),
      IsEditable(node));
}

void BookmarksBridge::ExtractBookmarkNodeInformation(
    const BookmarkNode* node,
    jobject j_result_obj) {
  JNIEnv* env = AttachCurrentThread();
  if (!IsReachable(node))
    return;
  Java_BookmarksBridge_addToList(
      env,
      j_result_obj,
      CreateJavaBookmark(node).obj());
}

const BookmarkNode* BookmarksBridge::GetNodeByID(long node_id,
                                                 int type) {
  const BookmarkNode* node;
  if (type == kBookmarkTypeManaged) {
    node = managed_bookmarks_shim_->GetNodeByID(
        static_cast<int64>(node_id));
  } else if (type == kBookmarkTypePartner) {
    node = partner_bookmarks_shim_->GetNodeByID(
        static_cast<int64>(node_id));
  } else {
    node = bookmark_model_->GetNodeByID(static_cast<int64>(node_id));
  }
  return node;
}

const BookmarkNode* BookmarksBridge::GetFolderWithFallback(
    long folder_id, int type) {
  const BookmarkNode* folder = GetNodeByID(folder_id, type);
  if (!folder || folder->type() == BookmarkNode::URL)
    folder = bookmark_model_->mobile_node();
  return folder;
}

bool BookmarksBridge::IsEditable(const BookmarkNode* node) const {
  if (!node || (node->type() != BookmarkNode::FOLDER &&
      node->type() != BookmarkNode::URL)) {
    return false;
  }
  if (!IsEditBookmarksEnabled())
    return false;
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return true;
  return !managed_bookmarks_shim_->IsManagedBookmark(node);
}

const BookmarkNode* BookmarksBridge::GetParentNode(const BookmarkNode* node) {
  DCHECK(IsLoaded());
  if (node == managed_bookmarks_shim_->GetManagedBookmarksRoot() ||
      node == partner_bookmarks_shim_->GetPartnerBookmarksRoot()) {
    return bookmark_model_->mobile_node();
  } else {
    return node->parent();
  }
}

int BookmarksBridge::GetBookmarkType(const BookmarkNode* node) {
  if (managed_bookmarks_shim_->IsManagedBookmark(node))
    return kBookmarkTypeManaged;
  else if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return kBookmarkTypePartner;
  else
    return kBookmarkTypeNormal;
}

string16 BookmarksBridge::GetTitle(const BookmarkNode* node) const {
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return partner_bookmarks_shim_->GetTitle(node);
  return node->GetTitle();
}

bool BookmarksBridge::IsReachable(const BookmarkNode* node) const {
  if (!partner_bookmarks_shim_->IsPartnerBookmark(node))
    return true;
  return partner_bookmarks_shim_->IsReachable(node);
}

bool BookmarksBridge::IsLoaded() const {
  return (bookmark_model_->loaded() && partner_bookmarks_shim_->IsLoaded());
}

void BookmarksBridge::NotifyIfDoneLoading() {
  if (!IsLoaded())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkModelLoaded(env, obj.obj());
}

// ------------- Observer-related methods ------------- //

void BookmarksBridge::BookmarkModelChanged() {
  if (!IsLoaded())
    return;

  // Called when there are changes to the bookmark model. It is most
  // likely changes to either managed or partner bookmarks.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkModelChanged(env, obj.obj());
}

void BookmarksBridge::Loaded(BookmarkModel* model, bool ids_reassigned) {
  NotifyIfDoneLoading();
}

void BookmarksBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

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
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_extensiveBookmarkChangesBeginning(env, obj.obj());
}

void BookmarksBridge::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_extensiveBookmarkChangesEnded(env, obj.obj());
}

void BookmarksBridge::OnManagedBookmarksChanged() {
  if (!IsLoaded())
    return;

  BookmarkModelChanged();
}

void BookmarksBridge::PartnerShimChanged(PartnerBookmarksShim* shim) {
  if (!IsLoaded())
    return;

  BookmarkModelChanged();
}

void BookmarksBridge::PartnerShimLoaded(PartnerBookmarksShim* shim) {
  NotifyIfDoneLoading();
}

void BookmarksBridge::ShimBeingDeleted(PartnerBookmarksShim* shim) {
  partner_bookmarks_shim_ = NULL;
}
