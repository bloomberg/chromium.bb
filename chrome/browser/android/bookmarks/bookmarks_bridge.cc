// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"

#include <stddef.h>
#include <stdint.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/stack_container.h"
#include "base/i18n/string_compare.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_match.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/undo_manager.h"
#include "content/public/browser/browser_thread.h"
#include "grit/components_strings.h"
#include "jni/BookmarksBridge_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaIntArray;
using bookmarks::android::JavaBookmarkIdCreateBookmarkId;
using bookmarks::android::JavaBookmarkIdGetId;
using bookmarks::android::JavaBookmarkIdGetType;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkPermanentNode;
using bookmarks::BookmarkType;
using content::BrowserThread;

namespace {

class BookmarkNodeCreationTimeCompareFunctor {
 public:
  bool operator()(const BookmarkNode* lhs, const BookmarkNode* rhs) {
    return lhs->date_added().ToJavaTime() > rhs->date_added().ToJavaTime();
  }
};

class BookmarkTitleComparer {
 public:
  explicit BookmarkTitleComparer(BookmarksBridge* bookmarks_bridge,
                                 const icu::Collator* collator)
      : bookmarks_bridge_(bookmarks_bridge),
        collator_(collator) {}

  bool operator()(const BookmarkNode* lhs, const BookmarkNode* rhs) {
    if (collator_) {
      return base::i18n::CompareString16WithCollator(
                 *collator_, bookmarks_bridge_->GetTitle(lhs),
                 bookmarks_bridge_->GetTitle(rhs)) == UCOL_LESS;
    } else {
      return lhs->GetTitle() < rhs->GetTitle();
    }
  }

private:
  BookmarksBridge* bookmarks_bridge_;  // weak
  const icu::Collator* collator_;
};

scoped_ptr<icu::Collator> GetICUCollator() {
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator_;
  collator_.reset(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator_.reset(NULL);

  return collator_;
}

}  // namespace

BookmarksBridge::BookmarksBridge(JNIEnv* env, jobject obj, jobject j_profile)
    : weak_java_ref_(env, obj),
      bookmark_model_(NULL),
      managed_bookmark_service_(NULL),
      partner_bookmarks_shim_(NULL) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  profile_ = ProfileAndroid::FromProfileAndroid(j_profile);
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile_);
  managed_bookmark_service_ =
      ManagedBookmarkServiceFactory::GetForProfile(profile_);

  // Registers the notifications we are interested.
  bookmark_model_->AddObserver(this);

  // Create the partner Bookmarks shim as early as possible (but don't attach).
  partner_bookmarks_shim_ = PartnerBookmarksShim::BuildForBrowserContext(
      chrome::GetBrowserContextRedirectedInIncognito(profile_));
  partner_bookmarks_shim_->AddObserver(this);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::Bind(&BookmarksBridge::EditBookmarksEnabledChanged,
                 base::Unretained(this)));

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

void BookmarksBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

// static
bool BookmarksBridge::RegisterBookmarksBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  BookmarksBridge* delegate = new BookmarksBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(delegate);
}

jboolean BookmarksBridge::IsEditBookmarksEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsEditBookmarksEnabled();
}

void BookmarksBridge::LoadEmptyPartnerBookmarkShimForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (partner_bookmarks_shim_->IsLoaded())
      return;
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(
      new BookmarkPermanentNode(0));
  DCHECK(partner_bookmarks_shim_->IsLoaded());
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetBookmarkByID(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong id,
    jint type) {
  DCHECK(IsLoaded());
  const BookmarkNode* node = GetNodeByID(id, type);
  return node ? CreateJavaBookmark(node) : ScopedJavaLocalRef<jobject>();
}

bool BookmarksBridge::IsDoingExtensiveChanges(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return bookmark_model_->IsDoingExtensiveChanges();
}

void BookmarksBridge::GetPermanentNodeIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  // TODO(kkimlabs): Remove this function.
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

void BookmarksBridge::GetTopLevelFolderParentIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  Java_BookmarksBridge_addToBookmarkIdList(
      env, j_result_obj, bookmark_model_->root_node()->id(),
      GetBookmarkType(bookmark_model_->root_node()));
}

void BookmarksBridge::GetTopLevelFolderIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean get_special,
    jboolean get_normal,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  std::vector<const BookmarkNode*> top_level_folders;

  if (get_special) {
    if (managed_bookmark_service_->managed_node() &&
        managed_bookmark_service_->managed_node()->child_count() > 0) {
      top_level_folders.push_back(managed_bookmark_service_->managed_node());
    }
    if (managed_bookmark_service_->supervised_node() &&
        managed_bookmark_service_->supervised_node()->child_count() > 0) {
      top_level_folders.push_back(managed_bookmark_service_->supervised_node());
    }
    if (partner_bookmarks_shim_->HasPartnerBookmarks()
        && IsReachable(partner_bookmarks_shim_->GetPartnerBookmarksRoot())) {
      top_level_folders.push_back(
          partner_bookmarks_shim_->GetPartnerBookmarksRoot());
    }
  }
  std::size_t special_count = top_level_folders.size();

  if (get_normal) {
    DCHECK_EQ(bookmark_model_->root_node()->child_count(), 5);

    const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
    for (int i = 0; i < mobile_node->child_count(); ++i) {
      const BookmarkNode* node = mobile_node->GetChild(i);
      if (node->is_folder()) {
        top_level_folders.push_back(node);
      }
    }

    const BookmarkNode* bookmark_bar_node =
        bookmark_model_->bookmark_bar_node();
    for (int i = 0; i < bookmark_bar_node->child_count(); ++i) {
      const BookmarkNode* node = bookmark_bar_node->GetChild(i);
      if (node->is_folder()) {
        top_level_folders.push_back(node);
      }
    }

    const BookmarkNode* other_node = bookmark_model_->other_node();
    for (int i = 0; i < other_node->child_count(); ++i) {
      const BookmarkNode* node = other_node->GetChild(i);
      if (node->is_folder()) {
        top_level_folders.push_back(node);
      }
    }

    scoped_ptr<icu::Collator> collator = GetICUCollator();
    std::stable_sort(top_level_folders.begin() + special_count,
                     top_level_folders.end(),
                     BookmarkTitleComparer(this, collator.get()));
  }

  for (std::vector<const BookmarkNode*>::const_iterator it =
      top_level_folders.begin(); it != top_level_folders.end(); ++it) {
    Java_BookmarksBridge_addToBookmarkIdList(env,
                                             j_result_obj,
                                             (*it)->id(),
                                             GetBookmarkType(*it));
  }
}

void BookmarksBridge::GetAllFoldersWithDepths(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folders_obj,
    const JavaParamRef<jobject>& j_depths_obj) {
  DCHECK(IsLoaded());

  scoped_ptr<icu::Collator> collator = GetICUCollator();

  // Vector to temporarily contain all child bookmarks at same level for sorting
  std::vector<const BookmarkNode*> bookmarkList;

  // Stack for Depth-First Search of bookmark model. It stores nodes and their
  // heights.
  std::stack<std::pair<const BookmarkNode*, int> > stk;

  bookmarkList.push_back(bookmark_model_->mobile_node());
  bookmarkList.push_back(bookmark_model_->bookmark_bar_node());
  bookmarkList.push_back(bookmark_model_->other_node());

  // Push all sorted top folders in stack and give them depth of 0.
  // Note the order to push folders to stack should be opposite to the order in
  // output.
  for (std::vector<const BookmarkNode*>::reverse_iterator it =
           bookmarkList.rbegin();
       it != bookmarkList.rend();
       ++it) {
    stk.push(std::make_pair(*it, 0));
  }

  while (!stk.empty()) {
    const BookmarkNode* node = stk.top().first;
    int depth = stk.top().second;
    stk.pop();
    Java_BookmarksBridge_addToBookmarkIdListWithDepth(env,
                                                      j_folders_obj,
                                                      node->id(),
                                                      GetBookmarkType(node),
                                                      j_depths_obj,
                                                      depth);
    bookmarkList.clear();
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child = node->GetChild(i);
      if (child->is_folder() &&
          managed_bookmark_service_->CanBeEditedByUser(child)) {
        bookmarkList.push_back(node->GetChild(i));
      }
    }
    std::stable_sort(bookmarkList.begin(),
                     bookmarkList.end(),
                     BookmarkTitleComparer(this, collator.get()));
    for (std::vector<const BookmarkNode*>::reverse_iterator it =
             bookmarkList.rbegin();
         it != bookmarkList.rend();
         ++it) {
      stk.push(std::make_pair(*it, depth + 1));
    }
  }
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetRootFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const BookmarkNode* root_node = bookmark_model_->root_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, root_node->id(), GetBookmarkType(root_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetMobileFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, mobile_node->id(), GetBookmarkType(mobile_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetOtherFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const BookmarkNode* other_node = bookmark_model_->other_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, other_node->id(), GetBookmarkType(other_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetDesktopFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const BookmarkNode* desktop_node = bookmark_model_->bookmark_bar_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, desktop_node->id(), GetBookmarkType(desktop_node));
  return folder_id_obj;
}

jint BookmarksBridge::GetChildCount(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong id,
                                    jint type) {
  DCHECK(IsLoaded());
  const BookmarkNode* node = GetNodeByID(id, type);
  return node->child_count();
}

void BookmarksBridge::GetChildIDs(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong id,
                                  jint type,
                                  jboolean get_folders,
                                  jboolean get_bookmarks,
                                  const JavaParamRef<jobject>& j_result_obj) {
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
        BookmarkType::BOOKMARK_TYPE_PARTNER);
  }
}

ScopedJavaLocalRef<jobject> BookmarksBridge::GetChildAt(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong id,
    jint type,
    jint index) {
  DCHECK(IsLoaded());

  const BookmarkNode* parent = GetNodeByID(id, type);
  DCHECK(parent);
  const BookmarkNode* child = parent->GetChild(index);
  return JavaBookmarkIdCreateBookmarkId(
      env, child->id(), GetBookmarkType(child));
}

void BookmarksBridge::GetAllBookmarkIDsOrderedByCreationDate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  std::list<const BookmarkNode*> folders;
  std::vector<const BookmarkNode*> result;
  folders.push_back(bookmark_model_->root_node());

  for (std::list<const BookmarkNode*>::iterator folder_iter = folders.begin();
      folder_iter != folders.end(); ++folder_iter) {
    if (*folder_iter == NULL)
      continue;

    std::list<const BookmarkNode*>::iterator insert_iter = folder_iter;
    ++insert_iter;

    for (int i = 0; i < (*folder_iter)->child_count(); ++i) {
      const BookmarkNode* child = (*folder_iter)->GetChild(i);
      if (!IsReachable(child) ||
          bookmarks::IsDescendantOf(
              child, managed_bookmark_service_->managed_node()) ||
          bookmarks::IsDescendantOf(
              child, managed_bookmark_service_->supervised_node())) {
        continue;
      }

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
                                       const JavaParamRef<jobject>& obj,
                                       jlong id,
                                       jint type,
                                       const JavaParamRef<jstring>& j_title) {
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
                                     const JavaParamRef<jobject>& obj,
                                     jlong id,
                                     jint type,
                                     const JavaParamRef<jstring>& url) {
  DCHECK(IsLoaded());
  bookmark_model_->SetURL(
      GetNodeByID(id, type),
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

bool BookmarksBridge::DoesBookmarkExist(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jlong id,
                                        jint type) {
  DCHECK(IsLoaded());

  const BookmarkNode* node = GetNodeByID(id, type);

  if (!node)
    return false;

  if (type == BookmarkType::BOOKMARK_TYPE_NORMAL) {
    return true;
  } else {
    DCHECK(type == BookmarkType::BOOKMARK_TYPE_PARTNER);
    return partner_bookmarks_shim_->IsReachable(node);
  }
}

void BookmarksBridge::GetBookmarksForFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folder_id_obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = JavaBookmarkIdGetId(env, j_folder_id_obj);
  int type = JavaBookmarkIdGetType(env, j_folder_id_obj);
  const BookmarkNode* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Recreate the java bookmarkId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, folder->id(), GetBookmarkType(folder));

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
        env, j_callback_obj, folder_id_obj.obj(), j_result_obj);
  }
}

jboolean BookmarksBridge::IsFolderVisible(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jlong id,
                                          jint type) {
  if (type == BookmarkType::BOOKMARK_TYPE_NORMAL) {
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
        bookmark_model_, static_cast<int64_t>(id));
    return node->IsVisible();
  } else if (type == BookmarkType::BOOKMARK_TYPE_PARTNER) {
    const BookmarkNode* node = partner_bookmarks_shim_->GetNodeByID(
        static_cast<long>(id));
    return partner_bookmarks_shim_->IsReachable(node);
  }

  NOTREACHED();
  return false;
}

void BookmarksBridge::GetCurrentFolderHierarchy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folder_id_obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = JavaBookmarkIdGetId(env, j_folder_id_obj);
  int type = JavaBookmarkIdGetType(env, j_folder_id_obj);
  const BookmarkNode* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Recreate the java bookmarkId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, folder->id(), GetBookmarkType(folder));

  // Get the folder hierarchy.
  const BookmarkNode* node = folder;
  while (node) {
    ExtractBookmarkNodeInformation(node, j_result_obj);
    node = GetParentNode(node);
  }

  Java_BookmarksCallback_onBookmarksFolderHierarchyAvailable(
      env, j_callback_obj, folder_id_obj.obj(), j_result_obj);
}

void BookmarksBridge::SearchBookmarks(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jobject>& j_list,
                                      const JavaParamRef<jstring>& j_query,
                                      jint max_results) {
  DCHECK(bookmark_model_->loaded());

  std::vector<bookmarks::BookmarkMatch> results;
  bookmark_model_->GetBookmarksMatching(
      base::android::ConvertJavaStringToUTF16(env, j_query),
      max_results,
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
      &results);
  for (const bookmarks::BookmarkMatch& match : results) {
    const BookmarkNode* node = match.node;

    std::vector<int> title_match_start_positions;
    std::vector<int> title_match_end_positions;
    for (auto position : match.title_match_positions) {
      title_match_start_positions.push_back(position.first);
      title_match_end_positions.push_back(position.second);
    }

    std::vector<int> url_match_start_positions;
    std::vector<int> url_match_end_positions;
    for (auto position : match.url_match_positions) {
      url_match_start_positions.push_back(position.first);
      url_match_end_positions.push_back(position.second);
    }

    Java_BookmarksBridge_addToBookmarkMatchList(
        env, j_list, node->id(), node->type(),
        ToJavaIntArray(env, title_match_start_positions).obj(),
        ToJavaIntArray(env, title_match_end_positions).obj(),
        ToJavaIntArray(env, url_match_start_positions).obj(),
        ToJavaIntArray(env, url_match_end_positions).obj());
  }
}

ScopedJavaLocalRef<jobject> BookmarksBridge::AddFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_title) {
  DCHECK(IsLoaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  const BookmarkNode* new_node = bookmark_model_->AddFolder(
      parent, index, base::android::ConvertJavaStringToUTF16(env, j_title));
  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, new_node->id(), GetBookmarkType(new_node));
  return new_java_obj;
}

void BookmarksBridge::DeleteBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_bookmark_id_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  int type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }

  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    partner_bookmarks_shim_->RemoveBookmark(node);
  else
    bookmark_model_->Remove(node);
}

void BookmarksBridge::MoveBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_bookmark_id_obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  int type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }
  bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* new_parent_node = GetNodeByID(bookmark_id, type);
  bookmark_model_->Move(node, new_parent_node, index);
}

ScopedJavaLocalRef<jobject> BookmarksBridge::AddBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jstring>& j_url) {
  DCHECK(IsLoaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  const BookmarkNode* new_node = bookmark_model_->AddURL(
      parent,
      index,
      base::android::ConvertJavaStringToUTF16(env, j_title),
      GURL(base::android::ConvertJavaStringToUTF16(env, j_url)));
  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj =
      JavaBookmarkIdCreateBookmarkId(
          env, new_node->id(), GetBookmarkType(new_node));
  return new_java_obj;
}

void BookmarksBridge::Undo(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  BookmarkUndoService* undo_service =
      BookmarkUndoServiceFactory::GetForProfile(profile_);
  UndoManager* undo_manager = undo_service->undo_manager();
  undo_manager->Undo();
}

void BookmarksBridge::StartGroupingUndos(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  DCHECK(!grouped_bookmark_actions_.get()); // shouldn't have started already
  grouped_bookmark_actions_.reset(
      new bookmarks::ScopedGroupBookmarkActions(bookmark_model_));
}

void BookmarksBridge::EndGroupingUndos(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  DCHECK(grouped_bookmark_actions_.get()); // should only call after start
  grouped_bookmark_actions_.reset();
}

base::string16 BookmarksBridge::GetTitle(const BookmarkNode* node) const {
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return partner_bookmarks_shim_->GetTitle(node);

  if (node == bookmark_model_->bookmark_bar_node()) {
    return l10n_util::GetStringUTF16(IDS_ENHANCED_BOOKMARK_BAR_FOLDER_NAME);
  }

  return node->GetTitle();
}

ScopedJavaLocalRef<jobject> BookmarksBridge::CreateJavaBookmark(
    const BookmarkNode* node) {
  JNIEnv* env = AttachCurrentThread();

  const BookmarkNode* parent = GetParentNode(node);
  int64_t parent_id = parent ? parent->id() : -1;

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
  if (type == BookmarkType::BOOKMARK_TYPE_PARTNER) {
    node = partner_bookmarks_shim_->GetNodeByID(static_cast<int64_t>(node_id));
  } else {
    node = bookmarks::GetBookmarkNodeByID(bookmark_model_,
                                          static_cast<int64_t>(node_id));
  }
  return node;
}

const BookmarkNode* BookmarksBridge::GetFolderWithFallback(long folder_id,
                                                           int type) {
  const BookmarkNode* folder = GetNodeByID(folder_id, type);
  if (!folder || folder->type() == BookmarkNode::URL ||
      !IsFolderAvailable(folder)) {
    if (!managed_bookmark_service_->managed_node()->empty())
      folder = managed_bookmark_service_->managed_node();
    else
      folder = bookmark_model_->mobile_node();
  }
  return folder;
}

bool BookmarksBridge::IsEditBookmarksEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled);
}

void BookmarksBridge::EditBookmarksEnabledChanged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_editBookmarksEnabledChanged(env, obj.obj());
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
  return managed_bookmark_service_->CanBeEditedByUser(node);
}

bool BookmarksBridge::IsManaged(const BookmarkNode* node) const {
  return bookmarks::IsDescendantOf(node,
                                   managed_bookmark_service_->managed_node());
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
    return BookmarkType::BOOKMARK_TYPE_PARTNER;
  else
    return BookmarkType::BOOKMARK_TYPE_NORMAL;
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
  if (folder == managed_bookmark_service_->managed_node() && folder->empty())
    return false;
  // Similarly, the supervised bookmarks folder is not shown if there are no
  // bookmarks configured by the custodian.
  if (folder == managed_bookmark_service_->supervised_node() && folder->empty())
    return false;

  SigninManager* signin = SigninManagerFactory::GetForProfile(
      profile_->GetOriginalProfile());
  return (folder->type() != BookmarkNode::BOOKMARK_BAR &&
      folder->type() != BookmarkNode::OTHER_NODE) ||
      (signin && signin->IsAuthenticated());
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

void BookmarksBridge::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_BookmarksBridge_bookmarkAllUserNodesRemoved(env, obj.obj());
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
