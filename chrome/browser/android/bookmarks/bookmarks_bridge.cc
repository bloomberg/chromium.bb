// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"

#include "base/android/jni_string.h"
#include "base/containers/stack_container.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "jni/BookmarksBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

// Should mirror constants in BookmarksBridge.java
static const int kBookmarkTypeNormal = 0;
static const int kBookmarkTypePartner = 1;

namespace {
class BookmarkNodeCreationTimeCompareFunctor {
 public:
  bool operator()(const BookmarkNode* lhs, const BookmarkNode* rhs) {
    return lhs->date_added().ToJavaTime() < rhs->date_added().ToJavaTime();
  }
};
}  // namespace

BookmarksBridge::BookmarksBridge(JNIEnv* env,
                                 jobject obj,
                                 jobject j_profile)
    : weak_java_ref_(env, obj),
      bookmark_model_(NULL),
      client_(NULL),
      partner_bookmarks_shim_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = ProfileAndroid::FromProfileAndroid(j_profile);
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile_);
  client_ = ChromeBookmarkClientFactory::GetForProfile(profile_);

  // Registers the notifications we are interested.
  bookmark_model_->AddObserver(this);

  // Create the partner Bookmarks shim as early as possible (but don't attach).
  partner_bookmarks_shim_ = PartnerBookmarksShim::BuildForBrowserContext(
      chrome::GetBrowserContextRedirectedInIncognito(profile_));
  partner_bookmarks_shim_->AddObserver(this);

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
}

void BookmarksBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

// static
bool BookmarksBridge::RegisterBookmarksBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, jobject obj, jobject j_profile) {
  BookmarksBridge* delegate = new BookmarksBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(delegate);
}

static bool IsEditBookmarksEnabled() {
  return ProfileManager::GetLastUsedProfile()->GetPrefs()->GetBoolean(
      prefs::kEditBookmarksEnabled);
}

static jboolean IsEditBookmarksEnabled(JNIEnv* env, jclass clazz) {
  return IsEditBookmarksEnabled();
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetBookmarkByID(JNIEnv* env,
                                                             jobject obj,
                                                             jlong id,
                                                             jint type) {
  DCHECK(IsLoaded());
  return CreateJavaBookmark(GetNodeByID(id, type));
}

void BookmarksBridge::GetPermanentNodeIDs(JNIEnv* env,
                                          jobject obj,
                                          jobject j_result_obj) {
  DCHECK(IsLoaded());

  base::StackVector<const BookmarkNode*, 8> permanent_nodes;

  // Save all the permanent nodes.
  const BookmarkNode* root_node = bookmark_model_->root_node();
  permanent_nodes->push_back(root_node);
  for (int i = 0; i < root_node->child_count(); ++i) {
    permanent_nodes->push_back(root_node->GetChild(i));
  }
  permanent_nodes->push_back(
      partner_bookmarks_shim_->GetPartnerBookmarksRoot());

  // Write the permanent nodes to |j_result_obj|.
  for (base::StackVector<const BookmarkNode*, 8>::ContainerType::const_iterator
           it = permanent_nodes->begin();
       it != permanent_nodes->end();
       ++it) {
    if (*it != NULL) {
      Java_BookmarksBridge_addToBookmarkIdList(
          env, j_result_obj, (*it)->id(), GetBookmarkType(*it));
    }
  }
}

void BookmarksBridge::GetChildIDs(JNIEnv* env,
                                  jobject obj,
                                  jlong id,
                                  jint type,
                                  jboolean get_folders,
                                  jboolean get_bookmarks,
                                  jobject j_result_obj) {
  DCHECK(IsLoaded());

  const BookmarkNode* parent = GetNodeByID(id, type);
  if (!parent->is_folder() || !IsReachable(parent))
    return;

  // Get the folder contents
  for (int i = 0; i < parent->child_count(); ++i) {
    const BookmarkNode* child = parent->GetChild(i);
    if (!IsFolderAvailable(child) || !IsReachable(child))
      continue;

    if ((child->is_folder() && get_folders) ||
        (!child->is_folder() && get_bookmarks)) {
      Java_BookmarksBridge_addToBookmarkIdList(
          env, j_result_obj, child->id(), GetBookmarkType(child));
    }
  }

  // Partner bookmark root node is under mobile node.
  if (parent == bookmark_model_->mobile_node() && get_folders &&
      partner_bookmarks_shim_->HasPartnerBookmarks() &&
      IsReachable(partner_bookmarks_shim_->GetPartnerBookmarksRoot())) {
    Java_BookmarksBridge_addToBookmarkIdList(
        env,
        j_result_obj,
        partner_bookmarks_shim_->GetPartnerBookmarksRoot()->id(),
        kBookmarkTypePartner);
  }
}

void BookmarksBridge::GetAllBookmarkIDsOrderedByCreationDate(
    JNIEnv* env,
    jobject obj,
    jobject j_result_obj) {
  DCHECK(IsLoaded());
  std::list<const BookmarkNode*> folders;
  std::vector<const BookmarkNode*> result;
  folders.push_back(bookmark_model_->root_node());
  folders.push_back(partner_bookmarks_shim_->GetPartnerBookmarksRoot());

  for (std::list<const BookmarkNode*>::iterator folder_iter = folders.begin();
      folder_iter != folders.end(); ++folder_iter) {
    if (*folder_iter == NULL)
      continue;

    std::list<const BookmarkNode*>::iterator insert_iter = folder_iter;
    ++insert_iter;

    for (int i = 0; i < (*folder_iter)->child_count(); ++i) {
      const BookmarkNode* child = (*folder_iter)->GetChild(i);
      if (!IsFolderAvailable(child) || !IsReachable(child))
        continue;

      if (child->is_folder()) {
        insert_iter = folders.insert(insert_iter, child);
      } else {
        result.push_back(child);
      }
    }
  }

  std::sort(
      result.begin(), result.end(), BookmarkNodeCreationTimeCompareFunctor());

  for (std::vector<const BookmarkNode*>::const_iterator iter = result.begin();
       iter != result.end();
       ++iter) {
    const BookmarkNode* bookmark = *iter;
    Java_BookmarksBridge_addToBookmarkIdList(
        env, j_result_obj, bookmark->id(), GetBookmarkType(bookmark));
  }
}

void BookmarksBridge::SetBookmarkTitle(JNIEnv* env,
                                       jobject obj,
                                       jlong id,
                                       jint type,
                                       jstring j_title) {
  DCHECK(IsLoaded());
  const BookmarkNode* bookmark = GetNodeByID(id, type);
  const base::string16 title =
      base::android::ConvertJavaStringToUTF16(env, j_title);

  if (partner_bookmarks_shim_->IsPartnerBookmark(bookmark)) {
    partner_bookmarks_shim_->RenameBookmark(bookmark, title);
  } else {
    bookmark_model_->SetTitle(bookmark, title);
  }
}

void BookmarksBridge::SetBookmarkUrl(JNIEnv* env,
                                     jobject obj,
                                     jlong id,
                                     jint type,
                                     jstring url) {
  DCHECK(IsLoaded());
  bookmark_model_->SetURL(
      GetNodeByID(id, type),
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

bool BookmarksBridge::DoesBookmarkExist(JNIEnv* env,
                                        jobject obj,
                                        jlong id,
                                        jint type) {
  DCHECK(IsLoaded());
  return GetNodeByID(id, type);
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

  // Recreate the java bookmarkId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      Java_BookmarksBridge_createBookmarkId(
          env, folder->id(), GetBookmarkType(folder));
  j_folder_id_obj = folder_id_obj.obj();

  // Get the folder contents.
  for (int i = 0; i < folder->child_count(); ++i) {
    const BookmarkNode* node = folder->GetChild(i);
    if (!IsFolderAvailable(node))
      continue;
    ExtractBookmarkNodeInformation(node, j_result_obj);
  }

  if (folder == bookmark_model_->mobile_node() &&
      partner_bookmarks_shim_->HasPartnerBookmarks()) {
    ExtractBookmarkNodeInformation(
        partner_bookmarks_shim_->GetPartnerBookmarksRoot(),
        j_result_obj);
  }

  if (j_callback_obj) {
    Java_BookmarksCallback_onBookmarksAvailable(
        env, j_callback_obj, j_folder_id_obj, j_result_obj);
  }
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

  // Recreate the java bookmarkId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      Java_BookmarksBridge_createBookmarkId(
          env, folder->id(), GetBookmarkType(folder));
  j_folder_id_obj = folder_id_obj.obj();

  // Get the folder hierarchy.
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

void BookmarksBridge::MoveBookmark(JNIEnv* env,
                                   jobject obj,
                                   jobject j_bookmark_id_obj,
                                   jobject j_parent_id_obj,
                                   jint index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(IsLoaded());

  long bookmark_id = Java_BookmarkId_getId(env, j_bookmark_id_obj);
  int type = Java_BookmarkId_getType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }
  bookmark_id = Java_BookmarkId_getId(env, j_parent_id_obj);
  type = Java_BookmarkId_getType(env, j_parent_id_obj);
  const BookmarkNode* new_parent_node = GetNodeByID(bookmark_id, type);
  bookmark_model_->Move(node, new_parent_node, index);
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
      IsEditable(node),
      IsManaged(node));
}

void BookmarksBridge::ExtractBookmarkNodeInformation(const BookmarkNode* node,
                                                     jobject j_result_obj) {
  JNIEnv* env = AttachCurrentThread();
  if (!IsReachable(node))
    return;
  Java_BookmarksBridge_addToList(
      env, j_result_obj, CreateJavaBookmark(node).obj());
}

const BookmarkNode* BookmarksBridge::GetNodeByID(long node_id, int type) {
  const BookmarkNode* node;
  if (type == kBookmarkTypePartner) {
    node = partner_bookmarks_shim_->GetNodeByID(
        static_cast<int64>(node_id));
  } else {
    node = bookmarks::GetBookmarkNodeByID(bookmark_model_,
                                          static_cast<int64>(node_id));
  }
  return node;
}

const BookmarkNode* BookmarksBridge::GetFolderWithFallback(long folder_id,
                                                           int type) {
  const BookmarkNode* folder = GetNodeByID(folder_id, type);
  if (!folder || folder->type() == BookmarkNode::URL ||
      !IsFolderAvailable(folder)) {
    if (!client_->managed_node()->empty())
      folder = client_->managed_node();
    else
      folder = bookmark_model_->mobile_node();
  }
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
    return partner_bookmarks_shim_->IsEditable(node);
  return client_->CanBeEditedByUser(node);
}

bool BookmarksBridge::IsManaged(const BookmarkNode* node) const {
  return client_->IsDescendantOfManagedNode(node);
}

const BookmarkNode* BookmarksBridge::GetParentNode(const BookmarkNode* node) {
  DCHECK(IsLoaded());
  if (node == partner_bookmarks_shim_->GetPartnerBookmarksRoot()) {
    return bookmark_model_->mobile_node();
  } else {
    return node->parent();
  }
}

int BookmarksBridge::GetBookmarkType(const BookmarkNode* node) {
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return kBookmarkTypePartner;
  else
    return kBookmarkTypeNormal;
}

base::string16 BookmarksBridge::GetTitle(const BookmarkNode* node) const {
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

bool BookmarksBridge::IsFolderAvailable(
    const BookmarkNode* folder) const {
  // The managed bookmarks folder is not shown if there are no bookmarks
  // configured via policy.
  if (folder == client_->managed_node() && folder->empty())
    return false;

  SigninManager* signin = SigninManagerFactory::GetForProfile(
      profile_->GetOriginalProfile());
  return (folder->type() != BookmarkNode::BOOKMARK_BAR &&
      folder->type() != BookmarkNode::OTHER_NODE) ||
      (signin && !signin->GetAuthenticatedUsername().empty());
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
  // likely changes to the partner bookmarks.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkModelChanged(env, obj.obj());
}

void BookmarksBridge::BookmarkModelLoaded(BookmarkModel* model,
                                          bool ids_reassigned) {
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
                                          const BookmarkNode* node,
                                          const std::set<GURL>& removed_urls) {
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
