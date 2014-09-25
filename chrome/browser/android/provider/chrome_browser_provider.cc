// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/provider/chrome_browser_provider.h"

#include <cmath>
#include <list>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/android/provider/blocking_ui_thread_async_request.h"
#include "chrome/browser/android/provider/bookmark_model_observer_task.h"
#include "chrome/browser/android/provider/run_on_ui_thread_blocking.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/android/android_history_types.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "jni/ChromeBrowserProvider_jni.h"
#include "sql/statement.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

// After refactoring the following class hierarchy has been created in order
// to avoid repeating code again for the same basic kind of tasks, to enforce
// the correct thread usage and to prevent known race conditions and deadlocks.
//
// - RunOnUIThreadBlocking: auxiliary class to run methods in the UI thread
//   blocking the current one until finished. Because of the provider threading
//   expectations this cannot be used from the UI thread.
//
// - BookmarkModelTask: base class for all tasks that operate in any way with
//   the bookmark model. This class ensures that the model is loaded and
//   prevents possible deadlocks. Derived classes should make use of
//   RunOnUIThreadBlocking to perform any manipulation of the bookmark model in
//   the UI thread. The Run method of these tasks cannot be invoked directly
//   from the UI thread, but RunOnUIThread can be safely used from the UI
//   thread code of other BookmarkModelTasks.
//
// - AsyncServiceRequest: base class for any asynchronous requests made to a
//   Chromium service that require to block the current thread until completed.
//   Derived classes should make use of RunAsyncRequestOnUIThreadBlocking to
//   post their requests in the UI thread and return the results synchronously.
//   All derived classes MUST ALWAYS call RequestCompleted when receiving the
//   request response. These tasks cannot be invoked from the UI thread.
//
// - FaviconServiceTask: base class for asynchronous requests that make use of
//   Chromium's favicon service. See AsyncServiceRequest for more details.
//
// - HistoryProviderTask: base class for asynchronous requests that make use of
//   AndroidHistoryProviderService. See AsyncServiceRequest for mode details.
//
// - SearchTermTask: base class for asynchronous requests that involve the
//   search term API. Works in the same way as HistoryProviderTask.

namespace {

const char kDefaultUrlScheme[] = "http://";
const int64 kInvalidContentProviderId = 0;
const int64 kInvalidBookmarkId = -1;

// ------------- Java-related utility methods ------------- //

// Convert a BookmarkNode, |node|, to the java representation of a bookmark node
// stored in |*jnode|. Parent node information is optional.
void ConvertBookmarkNode(
    const BookmarkNode* node,
    const JavaRef<jobject>& parent_node,
    ScopedJavaGlobalRef<jobject>* jnode) {
  DCHECK(jnode);
  if (!node)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> url;
  if (node->is_url())
    url.Reset(ConvertUTF8ToJavaString(env, node->url().spec()));
  ScopedJavaLocalRef<jstring> title(
      ConvertUTF16ToJavaString(env, node->GetTitle()));

  jnode->Reset(
      Java_BookmarkNode_create(
          env, node->id(), (jint) node->type(), title.obj(), url.obj(),
          parent_node.obj()));
}

jlong ConvertJLongObjectToPrimitive(JNIEnv* env, jobject long_obj) {
  ScopedJavaLocalRef<jclass> jlong_clazz = GetClass(env, "java/lang/Long");
  jmethodID long_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jlong_clazz.obj(), "longValue", "()J");
  return env->CallLongMethod(long_obj, long_value, NULL);
}

jboolean ConvertJBooleanObjectToPrimitive(JNIEnv* env, jobject boolean_object) {
  ScopedJavaLocalRef<jclass> jboolean_clazz =
      GetClass(env, "java/lang/Boolean");
  jmethodID boolean_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jboolean_clazz.obj(), "booleanValue", "()Z");
  return env->CallBooleanMethod(boolean_object, boolean_value, NULL);
}

base::Time ConvertJlongToTime(jlong value) {
  return base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds((int64)value);
}

jint ConvertJIntegerToJint(JNIEnv* env, jobject integer_obj) {
  ScopedJavaLocalRef<jclass> jinteger_clazz =
      GetClass(env, "java/lang/Integer");
  jmethodID int_value = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, jinteger_clazz.obj(), "intValue", "()I");
  return env->CallIntMethod(integer_obj, int_value, NULL);
}

std::vector<base::string16> ConvertJStringArrayToString16Array(
    JNIEnv* env,
    jobjectArray array) {
  std::vector<base::string16> results;
  if (array) {
    jsize len = env->GetArrayLength(array);
    for (int i = 0; i < len; i++) {
      results.push_back(ConvertJavaStringToUTF16(env,
          static_cast<jstring>(env->GetObjectArrayElement(array, i))));
    }
  }
  return results;
}

// ------------- Utility methods used by tasks ------------- //

// Parse the given url and return a GURL, appending the default scheme
// if one is not present.
GURL ParseAndMaybeAppendScheme(const base::string16& url,
                               const char* default_scheme) {
  GURL gurl(url);
  if (!gurl.is_valid() && !gurl.has_scheme()) {
    base::string16 refined_url(base::ASCIIToUTF16(default_scheme));
    refined_url.append(url);
    gurl = GURL(refined_url);
  }
  return gurl;
}

const BookmarkNode* GetChildFolderByTitle(const BookmarkNode* parent,
                                          const base::string16& title) {
  for (int i = 0; i < parent->child_count(); ++i) {
    if (parent->GetChild(i)->is_folder() &&
        parent->GetChild(i)->GetTitle() == title) {
      return parent->GetChild(i);
    }
  }
  return NULL;
}

// ------------- Synchronous task classes ------------- //

// Utility task to add a bookmark.
class AddBookmarkTask : public BookmarkModelTask {
 public:
  explicit AddBookmarkTask(BookmarkModel* model) : BookmarkModelTask(model) {}

  int64 Run(const base::string16& title,
            const base::string16& url,
            const bool is_folder,
            const int64 parent_id) {
    int64 result = kInvalidBookmarkId;
    RunOnUIThreadBlocking::Run(
        base::Bind(&AddBookmarkTask::RunOnUIThread,
                   model(), title, url, is_folder, parent_id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const base::string16& title,
                            const base::string16& url,
                            const bool is_folder,
                            const int64 parent_id,
                            int64* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(result);
    GURL gurl = ParseAndMaybeAppendScheme(url, kDefaultUrlScheme);

    // Check if the bookmark already exists.
    const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(gurl);
    if (!node) {
      const BookmarkNode* parent_node = NULL;
      if (parent_id >= 0)
        parent_node = bookmarks::GetBookmarkNodeByID(model, parent_id);
      if (!parent_node)
        parent_node = model->bookmark_bar_node();

      if (is_folder)
        node = model->AddFolder(parent_node, parent_node->child_count(), title);
      else
        node = model->AddURL(parent_node, 0, title, gurl);
    }

    *result = node ? node ->id() : kInvalidBookmarkId;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddBookmarkTask);
};

// Utility method to remove a bookmark.
class RemoveBookmarkTask : public BookmarkModelObserverTask {
 public:
  explicit RemoveBookmarkTask(BookmarkModel* model)
      : BookmarkModelObserverTask(model),
        deleted_(0),
        id_to_delete_(kInvalidBookmarkId) {}
  virtual ~RemoveBookmarkTask() {}

  int Run(const int64 id) {
    id_to_delete_ = id;
    RunOnUIThreadBlocking::Run(
        base::Bind(&RemoveBookmarkTask::RunOnUIThread, model(), id));
    return deleted_;
  }

  static void RunOnUIThread(BookmarkModel* model, const int64 id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    if (node && node->parent()) {
      const BookmarkNode* parent_node = node->parent();
      model->Remove(parent_node, parent_node->GetIndexOf(node));
    }
  }

  // Verify that the bookmark was actually removed. Called synchronously.
  virtual void BookmarkNodeRemoved(
      BookmarkModel* bookmark_model,
      const BookmarkNode* parent,
      int old_index,
      const BookmarkNode* node,
      const std::set<GURL>& removed_urls) OVERRIDE {
    if (bookmark_model == model() && node->id() == id_to_delete_)
        ++deleted_;
  }

 private:
  int deleted_;
  int64 id_to_delete_;

  DISALLOW_COPY_AND_ASSIGN(RemoveBookmarkTask);
};

// Utility method to remove all bookmarks that the user can edit.
class RemoveAllUserBookmarksTask : public BookmarkModelObserverTask {
 public:
  explicit RemoveAllUserBookmarksTask(BookmarkModel* model)
      : BookmarkModelObserverTask(model) {}

  virtual ~RemoveAllUserBookmarksTask() {}

  void Run() {
    RunOnUIThreadBlocking::Run(
        base::Bind(&RemoveAllUserBookmarksTask::RunOnUIThread, model()));
  }

  static void RunOnUIThread(BookmarkModel* model) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    model->RemoveAllUserBookmarks();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveAllUserBookmarksTask);
};

// Utility method to update a bookmark.
class UpdateBookmarkTask : public BookmarkModelObserverTask {
 public:
  explicit UpdateBookmarkTask(BookmarkModel* model)
      : BookmarkModelObserverTask(model),
        updated_(0),
        id_to_update_(kInvalidBookmarkId){}
  virtual ~UpdateBookmarkTask() {}

  int Run(const int64 id,
          const base::string16& title,
          const base::string16& url,
          const int64 parent_id) {
    id_to_update_ = id;
    RunOnUIThreadBlocking::Run(
        base::Bind(&UpdateBookmarkTask::RunOnUIThread,
                   model(), id, title, url, parent_id));
    return updated_;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64 id,
                            const base::string16& title,
                            const base::string16& url,
                            const int64 parent_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    if (node) {
      if (node->GetTitle() != title)
        model->SetTitle(node, title);

      if (node->type() == BookmarkNode::URL) {
        GURL bookmark_url = ParseAndMaybeAppendScheme(url, kDefaultUrlScheme);
        if (bookmark_url != node->url())
          model->SetURL(node, bookmark_url);
      }

      if (parent_id >= 0 &&
          (!node->parent() || parent_id != node->parent()->id())) {
        const BookmarkNode* new_parent =
            bookmarks::GetBookmarkNodeByID(model, parent_id);

        if (new_parent)
          model->Move(node, new_parent, 0);
      }
    }
  }

  // Verify that the bookmark was actually updated. Called synchronously.
  virtual void BookmarkNodeChanged(BookmarkModel* bookmark_model,
                                   const BookmarkNode* node) OVERRIDE {
    if (bookmark_model == model() && node->id() == id_to_update_)
      ++updated_;
  }

 private:
  int updated_;
  int64 id_to_update_;

  DISALLOW_COPY_AND_ASSIGN(UpdateBookmarkTask);
};

// Checks if a node exists in the bookmark model.
class BookmarkNodeExistsTask : public BookmarkModelTask {
 public:
  explicit BookmarkNodeExistsTask(BookmarkModel* model)
      : BookmarkModelTask(model) {
  }

  bool Run(const int64 id) {
    bool result = false;
    RunOnUIThreadBlocking::Run(
        base::Bind(&BookmarkNodeExistsTask::RunOnUIThread,
                   model(), id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64 id,
                            bool* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(result);
    *result = bookmarks::GetBookmarkNodeByID(model, id) != NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeExistsTask);
};

// Checks if a node belongs to the Mobile Bookmarks hierarchy branch.
class IsInMobileBookmarksBranchTask : public BookmarkModelTask {
 public:
  explicit IsInMobileBookmarksBranchTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  bool Run(const int64 id) {
    bool result = false;
    RunOnUIThreadBlocking::Run(
        base::Bind(&IsInMobileBookmarksBranchTask::RunOnUIThread,
                   model(), id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64 id,
                            bool *result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(result);
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    const BookmarkNode* mobile_node = model->mobile_node();
    while (node && node != mobile_node)
      node = node->parent();

    *result = node == mobile_node;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsInMobileBookmarksBranchTask);
};

// Creates folder or retrieves its id if already exists.
// An invalid parent id is assumed to represent the Mobile Bookmarks folder.
// Can only be used to create folders inside the Mobile Bookmarks branch.
class CreateBookmarksFolderOnceTask : public BookmarkModelTask {
 public:
  explicit CreateBookmarksFolderOnceTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  int64 Run(const base::string16& title, const int64 parent_id) {
    int64 result = kInvalidBookmarkId;
    RunOnUIThreadBlocking::Run(
        base::Bind(&CreateBookmarksFolderOnceTask::RunOnUIThread,
                   model(), title, parent_id, &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const base::string16& title,
                            const int64 parent_id,
                            int64* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(result);

    // Invalid ids are assumed to refer to the Mobile Bookmarks folder.
    const BookmarkNode* parent =
        parent_id >= 0 ? bookmarks::GetBookmarkNodeByID(model, parent_id)
                       : model->mobile_node();
    DCHECK(parent);

    bool in_mobile_bookmarks;
    IsInMobileBookmarksBranchTask::RunOnUIThread(model, parent->id(),
                                                 &in_mobile_bookmarks);
    if (!in_mobile_bookmarks) {
      // The parent folder must be inside the Mobile Bookmarks folder.
      *result = kInvalidBookmarkId;
      return;
    }

    const BookmarkNode* node = GetChildFolderByTitle(parent, title);
    if (node) {
      *result = node->id();
      return;
    }

    AddBookmarkTask::RunOnUIThread(model, title, base::string16(), true,
                                   parent->id(), result);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateBookmarksFolderOnceTask);
};

// Creates a Java BookmarkNode object for a node given its id.
class GetEditableBookmarkFoldersTask : public BookmarkModelTask {
 public:
  GetEditableBookmarkFoldersTask(ChromeBookmarkClient* client,
                                 BookmarkModel* model)
      : BookmarkModelTask(model), client_(client) {}

  void Run(ScopedJavaGlobalRef<jobject>* jroot) {
    RunOnUIThreadBlocking::Run(
        base::Bind(&GetEditableBookmarkFoldersTask::RunOnUIThread,
                   client_, model(), jroot));
  }

  static void RunOnUIThread(ChromeBookmarkClient* client,
                            BookmarkModel* model,
                            ScopedJavaGlobalRef<jobject>* jroot) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const BookmarkNode* root = model->root_node();
    if (!root || !root->is_folder())
      return;

    // The iterative approach is not possible because ScopedGlobalJavaRefs
    // cannot be copy-constructed, and therefore not used in STL containers.
    ConvertFolderSubtree(client, AttachCurrentThread(), root,
                         ScopedJavaLocalRef<jobject>(), jroot);
  }

 private:
  static void ConvertFolderSubtree(ChromeBookmarkClient* client,
                                   JNIEnv* env,
                                   const BookmarkNode* node,
                                   const JavaRef<jobject>& parent_folder,
                                   ScopedJavaGlobalRef<jobject>* jfolder) {
    DCHECK(node);
    DCHECK(node->is_folder());
    DCHECK(jfolder);

    // Global refs should be used here for thread-safety reasons as this task
    // might be invoked from a thread other than UI. All refs are scoped.
    ConvertBookmarkNode(node, parent_folder, jfolder);

    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child = node->GetChild(i);
      if (child->is_folder() && client->CanBeEditedByUser(child)) {
        ScopedJavaGlobalRef<jobject> jchild;
        ConvertFolderSubtree(client, env, child, *jfolder, &jchild);

        Java_BookmarkNode_addChild(env, jfolder->obj(), jchild.obj());
        if (ClearException(env)) {
          LOG(WARNING) << "Java exception while adding child node.";
          return;
        }
      }
    }
  }

  ChromeBookmarkClient* client_;

  DISALLOW_COPY_AND_ASSIGN(GetEditableBookmarkFoldersTask);
};

// Creates a Java BookmarkNode object for a node given its id.
class GetBookmarkNodeTask : public BookmarkModelTask {
 public:
  explicit GetBookmarkNodeTask(BookmarkModel* model)
      : BookmarkModelTask(model) {
  }

  void Run(const int64 id,
           bool get_parent,
           bool get_children,
           ScopedJavaGlobalRef<jobject>* jnode) {
    return RunOnUIThreadBlocking::Run(
        base::Bind(&GetBookmarkNodeTask::RunOnUIThread,
                   model(), id, get_parent, get_children, jnode));
  }

  static void RunOnUIThread(BookmarkModel* model,
                            const int64 id,
                            bool get_parent,
                            bool get_children,
                            ScopedJavaGlobalRef<jobject>* jnode) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
    if (!node || !jnode)
      return;

    ScopedJavaGlobalRef<jobject> jparent;
    if (get_parent) {
      ConvertBookmarkNode(node->parent(), ScopedJavaLocalRef<jobject>(),
                          &jparent);
    }

    ConvertBookmarkNode(node, jparent, jnode);

    JNIEnv* env = AttachCurrentThread();
    if (!jparent.is_null()) {
      Java_BookmarkNode_addChild(env, jparent.obj(), jnode->obj());
      if (ClearException(env)) {
        LOG(WARNING) << "Java exception while adding child node.";
        return;
      }
    }

    if (get_children) {
      for (int i = 0; i < node->child_count(); ++i) {
        ScopedJavaGlobalRef<jobject> jchild;
        ConvertBookmarkNode(node->GetChild(i), *jnode, &jchild);
        Java_BookmarkNode_addChild(env, jnode->obj(), jchild.obj());
        if (ClearException(env)) {
          LOG(WARNING) << "Java exception while adding child node.";
          return;
        }
      }
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GetBookmarkNodeTask);
};

// Gets the Mobile Bookmarks node. Using this task ensures the correct
// initialization of the bookmark model.
class GetMobileBookmarksNodeTask : public BookmarkModelTask {
 public:
  explicit GetMobileBookmarksNodeTask(BookmarkModel* model)
      : BookmarkModelTask(model) {}

  const BookmarkNode* Run() {
    const BookmarkNode* result = NULL;
    RunOnUIThreadBlocking::Run(
        base::Bind(&GetMobileBookmarksNodeTask::RunOnUIThread,
                   model(), &result));
    return result;
  }

  static void RunOnUIThread(BookmarkModel* model, const BookmarkNode** result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(result);
    *result = model->mobile_node();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GetMobileBookmarksNodeTask);
};

// ------------- Aynchronous requests classes ------------- //

// Base class for asynchronous blocking requests to Chromium services.
// Service: type of the service to use (e.g. HistoryService, FaviconService).
template <typename Service>
class AsyncServiceRequest : protected BlockingUIThreadAsyncRequest {
 public:
  AsyncServiceRequest(Service* service,
                      base::CancelableTaskTracker* cancelable_tracker)
      : service_(service), cancelable_tracker_(cancelable_tracker) {}

  Service* service() const { return service_; }

  base::CancelableTaskTracker* cancelable_tracker() const {
    return cancelable_tracker_;
  }

 private:
  Service* service_;
  base::CancelableTaskTracker* cancelable_tracker_;

  DISALLOW_COPY_AND_ASSIGN(AsyncServiceRequest);
};

// Base class for all asynchronous blocking tasks that use the favicon service.
class FaviconServiceTask : public AsyncServiceRequest<FaviconService> {
 public:
  FaviconServiceTask(FaviconService* service,
                     base::CancelableTaskTracker* cancelable_tracker,
                     Profile* profile)
      : AsyncServiceRequest<FaviconService>(service, cancelable_tracker),
        profile_(profile) {}

  Profile* profile() const { return profile_; }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FaviconServiceTask);
};

// Retrieves the favicon or touch icon for a URL from the FaviconService.
class BookmarkIconFetchTask : public FaviconServiceTask {
 public:
  BookmarkIconFetchTask(FaviconService* favicon_service,
                        base::CancelableTaskTracker* cancelable_tracker,
                        Profile* profile)
      : FaviconServiceTask(favicon_service, cancelable_tracker, profile) {}

  favicon_base::FaviconRawBitmapResult Run(const GURL& url) {
    float max_scale = ui::GetScaleForScaleFactor(
        ResourceBundle::GetSharedInstance().GetMaxScaleFactor());
    int desired_size_in_pixel = std::ceil(gfx::kFaviconSize * max_scale);
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&FaviconService::GetRawFaviconForPageURL,
                   base::Unretained(service()),
                   url,
                   favicon_base::FAVICON | favicon_base::TOUCH_ICON,
                   desired_size_in_pixel,
                   base::Bind(&BookmarkIconFetchTask::OnFaviconRetrieved,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnFaviconRetrieved(
      const favicon_base::FaviconRawBitmapResult& bitmap_result) {
    result_ = bitmap_result;
    RequestCompleted();
  }

  favicon_base::FaviconRawBitmapResult result_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkIconFetchTask);
};

// Base class for all asynchronous blocking tasks that use the Android history
// provider service.
class HistoryProviderTask
    : public AsyncServiceRequest<AndroidHistoryProviderService> {
 public:
  HistoryProviderTask(AndroidHistoryProviderService* service,
                      base::CancelableTaskTracker* cancelable_tracker)
      : AsyncServiceRequest<AndroidHistoryProviderService>(service,
                                                           cancelable_tracker) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryProviderTask);
};

// Adds a bookmark from the API.
class AddBookmarkFromAPITask : public HistoryProviderTask {
 public:
  AddBookmarkFromAPITask(AndroidHistoryProviderService* service,
                         base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker) {}

  history::URLID Run(const history::HistoryAndBookmarkRow& row) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::InsertHistoryAndBookmark,
                   base::Unretained(service()),
                   row,
                   base::Bind(&AddBookmarkFromAPITask::OnBookmarkInserted,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarkInserted(history::URLID id) {
    // Note that here 0 means an invalid id.
    // This is because it represents a SQLite database row id.
    result_ = id;
    RequestCompleted();
  }

  history::URLID result_;

  DISALLOW_COPY_AND_ASSIGN(AddBookmarkFromAPITask);
};

// Queries bookmarks from the API.
class QueryBookmarksFromAPITask : public HistoryProviderTask {
 public:
  QueryBookmarksFromAPITask(AndroidHistoryProviderService* service,
                            base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(NULL) {}

  history::AndroidStatement* Run(
      const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::QueryHistoryAndBookmarks,
                   base::Unretained(service()),
                   projections,
                   selection,
                   selection_args,
                   sort_order,
                   base::Bind(&QueryBookmarksFromAPITask::OnBookmarksQueried,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksQueried(history::AndroidStatement* statement) {
    result_ = statement;
    RequestCompleted();
  }

  history::AndroidStatement* result_;

  DISALLOW_COPY_AND_ASSIGN(QueryBookmarksFromAPITask);
};

// Updates bookmarks from the API.
class UpdateBookmarksFromAPITask : public HistoryProviderTask {
 public:
  UpdateBookmarksFromAPITask(AndroidHistoryProviderService* service,
                             base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const history::HistoryAndBookmarkRow& row,
          const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::UpdateHistoryAndBookmarks,
                   base::Unretained(service()),
                   row,
                   selection,
                   selection_args,
                   base::Bind(&UpdateBookmarksFromAPITask::OnBookmarksUpdated,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksUpdated(int updated_row_count) {
    result_ = updated_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(UpdateBookmarksFromAPITask);
};

// Removes bookmarks from the API.
class RemoveBookmarksFromAPITask : public HistoryProviderTask {
 public:
  RemoveBookmarksFromAPITask(AndroidHistoryProviderService* service,
                             base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::DeleteHistoryAndBookmarks,
                   base::Unretained(service()),
                   selection,
                   selection_args,
                   base::Bind(&RemoveBookmarksFromAPITask::OnBookmarksRemoved,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnBookmarksRemoved(int removed_row_count) {
    result_ = removed_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveBookmarksFromAPITask);
};

// Removes history from the API.
class RemoveHistoryFromAPITask : public HistoryProviderTask {
 public:
  RemoveHistoryFromAPITask(AndroidHistoryProviderService* service,
                           base::CancelableTaskTracker* cancelable_tracker)
      : HistoryProviderTask(service, cancelable_tracker), result_(0) {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AndroidHistoryProviderService::DeleteHistory,
                   base::Unretained(service()),
                   selection,
                   selection_args,
                   base::Bind(&RemoveHistoryFromAPITask::OnHistoryRemoved,
                              base::Unretained(this)),
                   cancelable_tracker()));
    return result_;
  }

 private:
  void OnHistoryRemoved(int removed_row_count) {
    result_ = removed_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryFromAPITask);
};

// This class provides the common method for the SearchTermAPIHelper.
class SearchTermTask : public HistoryProviderTask {
 protected:
  SearchTermTask(AndroidHistoryProviderService* service,
                 base::CancelableTaskTracker* cancelable_tracker,
                 Profile* profile)
      : HistoryProviderTask(service, cancelable_tracker), profile_(profile) {}

  // Fill SearchRow's keyword_id and url fields according the given
  // search_term. Return true if succeeded.
  void BuildSearchRow(history::SearchRow* row) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    TemplateURLService* template_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    template_service->Load();

    const TemplateURL* search_engine =
        template_service->GetDefaultSearchProvider();
    if (search_engine) {
      const TemplateURLRef* search_url = &search_engine->url_ref();
      TemplateURLRef::SearchTermsArgs search_terms_args(row->search_term());
      search_terms_args.append_extra_query_params = true;
      std::string url = search_url->ReplaceSearchTerms(
          search_terms_args, template_service->search_terms_data());
      if (!url.empty()) {
        row->set_url(GURL(url));
        row->set_keyword_id(search_engine->id());
      }
    }
  }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SearchTermTask);
};

// Adds a search term from the API.
class AddSearchTermFromAPITask : public SearchTermTask {
 public:
  AddSearchTermFromAPITask(AndroidHistoryProviderService* service,
                           base::CancelableTaskTracker* cancelable_tracker,
                           Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile) {}

  history::URLID Run(const history::SearchRow& row) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&AddSearchTermFromAPITask::MakeRequestOnUIThread,
                   base::Unretained(this), row));
    return result_;
  }

 private:
  void MakeRequestOnUIThread(const history::SearchRow& row) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    history::SearchRow internal_row = row;
    BuildSearchRow(&internal_row);
    service()->InsertSearchTerm(
        internal_row,
        base::Bind(&AddSearchTermFromAPITask::OnSearchTermInserted,
                   base::Unretained(this)),
        cancelable_tracker());
  }

  void OnSearchTermInserted(history::URLID id) {
    // Note that here 0 means an invalid id.
    // This is because it represents a SQLite database row id.
    result_ = id;
    RequestCompleted();
  }

  history::URLID result_;

  DISALLOW_COPY_AND_ASSIGN(AddSearchTermFromAPITask);
};

// Queries search terms from the API.
class QuerySearchTermsFromAPITask : public SearchTermTask {
 public:
  QuerySearchTermsFromAPITask(AndroidHistoryProviderService* service,
                              base::CancelableTaskTracker* cancelable_tracker,
                              Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_(NULL) {}

  history::AndroidStatement* Run(
      const std::vector<history::SearchRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order) {
    RunAsyncRequestOnUIThreadBlocking(base::Bind(
        &AndroidHistoryProviderService::QuerySearchTerms,
        base::Unretained(service()),
        projections,
        selection,
        selection_args,
        sort_order,
        base::Bind(&QuerySearchTermsFromAPITask::OnSearchTermsQueried,
                   base::Unretained(this)),
        cancelable_tracker()));
    return result_;
  }

 private:
  // Callback to return the result.
  void OnSearchTermsQueried(history::AndroidStatement* statement) {
    result_ = statement;
    RequestCompleted();
  }

  history::AndroidStatement* result_;

  DISALLOW_COPY_AND_ASSIGN(QuerySearchTermsFromAPITask);
};

// Updates search terms from the API.
class UpdateSearchTermsFromAPITask : public SearchTermTask {
 public:
  UpdateSearchTermsFromAPITask(AndroidHistoryProviderService* service,
                               base::CancelableTaskTracker* cancelable_tracker,
                               Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_(0) {}

  int Run(const history::SearchRow& row,
          const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(
        base::Bind(&UpdateSearchTermsFromAPITask::MakeRequestOnUIThread,
                   base::Unretained(this), row, selection, selection_args));
    return result_;
  }

 private:
  void MakeRequestOnUIThread(
      const history::SearchRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    history::SearchRow internal_row = row;
    BuildSearchRow(&internal_row);
    service()->UpdateSearchTerms(
        internal_row,
        selection,
        selection_args,
        base::Bind(&UpdateSearchTermsFromAPITask::OnSearchTermsUpdated,
                   base::Unretained(this)),
        cancelable_tracker());
  }

  void OnSearchTermsUpdated(int updated_row_count) {
    result_ = updated_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(UpdateSearchTermsFromAPITask);
};

// Removes search terms from the API.
class RemoveSearchTermsFromAPITask : public SearchTermTask {
 public:
  RemoveSearchTermsFromAPITask(AndroidHistoryProviderService* service,
                               base::CancelableTaskTracker* cancelable_tracker,
                               Profile* profile)
      : SearchTermTask(service, cancelable_tracker, profile), result_() {}

  int Run(const std::string& selection,
          const std::vector<base::string16>& selection_args) {
    RunAsyncRequestOnUIThreadBlocking(base::Bind(
        &AndroidHistoryProviderService::DeleteSearchTerms,
        base::Unretained(service()),
        selection,
        selection_args,
        base::Bind(&RemoveSearchTermsFromAPITask::OnSearchTermsDeleted,
                   base::Unretained(this)),
        cancelable_tracker()));
    return result_;
  }

 private:
  void OnSearchTermsDeleted(int deleted_row_count) {
    result_ = deleted_row_count;
    RequestCompleted();
  }

  int result_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSearchTermsFromAPITask);
};

// ------------- Other utility methods (may use tasks) ------------- //

// Fills the bookmark |row| with the given java objects.
void FillBookmarkRow(JNIEnv* env,
                     jobject obj,
                     jstring url,
                     jobject created,
                     jobject isBookmark,
                     jobject date,
                     jbyteArray favicon,
                     jstring title,
                     jobject visits,
                     jlong parent_id,
                     history::HistoryAndBookmarkRow* row,
                     BookmarkModel* model) {
  // Needed because of the internal bookmark model task invocation.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (url) {
    base::string16 raw_url = ConvertJavaStringToUTF16(env, url);
    // GURL doesn't accept the URL without protocol, but the Android CTS
    // allows it. We are trying to prefix with 'http://' to see whether
    // GURL thinks it is a valid URL. The original url will be stored in
    // history::BookmarkRow.raw_url_.
    GURL gurl = ParseAndMaybeAppendScheme(raw_url, kDefaultUrlScheme);
    row->set_url(gurl);
    row->set_raw_url(base::UTF16ToUTF8(raw_url));
  }

  if (created)
    row->set_created(ConvertJlongToTime(
        ConvertJLongObjectToPrimitive(env, created)));

  if (isBookmark)
    row->set_is_bookmark(ConvertJBooleanObjectToPrimitive(env, isBookmark));

  if (date)
    row->set_last_visit_time(ConvertJlongToTime(ConvertJLongObjectToPrimitive(
        env, date)));

  if (favicon) {
    std::vector<uint8> bytes;
    base::android::JavaByteArrayToByteVector(env, favicon, &bytes);
    row->set_favicon(base::RefCountedBytes::TakeVector(&bytes));
  }

  if (title)
    row->set_title(ConvertJavaStringToUTF16(env, title));

  if (visits)
    row->set_visit_count(ConvertJIntegerToJint(env, visits));

  // Make sure parent_id is always in the mobile_node branch.
  IsInMobileBookmarksBranchTask task(model);
  if (task.Run(parent_id))
    row->set_parent_id(parent_id);
}

// Fills the bookmark |row| with the given java objects if it is not null.
void FillSearchRow(JNIEnv* env,
                   jobject obj,
                   jstring search_term,
                   jobject date,
                   history::SearchRow* row) {
  if (search_term)
    row->set_search_term(ConvertJavaStringToUTF16(env, search_term));

  if (date)
    row->set_search_time(ConvertJlongToTime(ConvertJLongObjectToPrimitive(
        env, date)));
}

}  // namespace

// ------------- Native initialization and destruction ------------- //

static jlong Init(JNIEnv* env, jobject obj) {
  ChromeBrowserProvider* provider = new ChromeBrowserProvider(env, obj);
  return reinterpret_cast<intptr_t>(provider);
}

bool ChromeBrowserProvider::RegisterChromeBrowserProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ChromeBrowserProvider::ChromeBrowserProvider(JNIEnv* env, jobject obj)
    : weak_java_provider_(env, obj),
      handling_extensive_changes_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = g_browser_process->profile_manager()->GetLastUsedProfile();
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile_);
  top_sites_ = profile_->GetTopSites();
  service_.reset(new AndroidHistoryProviderService(profile_));
  favicon_service_.reset(FaviconServiceFactory::GetForProfile(profile_,
      Profile::EXPLICIT_ACCESS));

  // Registers the notifications we are interested.
  bookmark_model_->AddObserver(this);
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED,
      content::NotificationService::AllSources());
  TemplateURLService* template_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_service->loaded())
    template_service->Load();
}

ChromeBrowserProvider::~ChromeBrowserProvider() {
  bookmark_model_->RemoveObserver(this);
}

void ChromeBrowserProvider::Destroy(JNIEnv*, jobject) {
  delete this;
}

// ------------- Provider public APIs ------------- //

jlong ChromeBrowserProvider::AddBookmark(JNIEnv* env,
                                         jobject,
                                         jstring jurl,
                                         jstring jtitle,
                                         jboolean is_folder,
                                         jlong parent_id) {
  base::string16 url;
  if (jurl)
    url = ConvertJavaStringToUTF16(env, jurl);
  base::string16 title = ConvertJavaStringToUTF16(env, jtitle);

  AddBookmarkTask task(bookmark_model_);
  return task.Run(title, url, is_folder, parent_id);
}

jint ChromeBrowserProvider::RemoveBookmark(JNIEnv*, jobject, jlong id) {
  RemoveBookmarkTask task(bookmark_model_);
  return task.Run(id);
}

jint ChromeBrowserProvider::UpdateBookmark(JNIEnv* env,
                                           jobject,
                                           jlong id,
                                           jstring jurl,
                                           jstring jtitle,
                                           jlong parent_id) {
  base::string16 url;
  if (jurl)
    url = ConvertJavaStringToUTF16(env, jurl);
  base::string16 title = ConvertJavaStringToUTF16(env, jtitle);

  UpdateBookmarkTask task(bookmark_model_);
  return task.Run(id, title, url, parent_id);
}

// Add the bookmark with the given column values.
jlong ChromeBrowserProvider::AddBookmarkFromAPI(JNIEnv* env,
                                                jobject obj,
                                                jstring url,
                                                jobject created,
                                                jobject isBookmark,
                                                jobject date,
                                                jbyteArray favicon,
                                                jstring title,
                                                jobject visits,
                                                jlong parent_id) {
  DCHECK(url);

  history::HistoryAndBookmarkRow row;
  FillBookmarkRow(env, obj, url, created, isBookmark, date, favicon, title,
                  visits, parent_id, &row, bookmark_model_);

  // URL must be valid.
  if (row.url().is_empty()) {
    LOG(ERROR) << "Not a valid URL " << row.raw_url();
    return kInvalidContentProviderId;
  }

  AddBookmarkFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(row);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::QueryBookmarkFromAPI(
    JNIEnv* env,
    jobject obj,
    jobjectArray projection,
    jstring selections,
    jobjectArray selection_args,
    jstring sort_order) {
  // Converts the projection to array of ColumnID and column name.
  // Used to store the projection column ID according their sequence.
  std::vector<history::HistoryAndBookmarkRow::ColumnID> query_columns;
  // Used to store the projection column names according their sequence.
  std::vector<std::string> columns_name;
  if (projection) {
    jsize len = env->GetArrayLength(projection);
    for (int i = 0; i < len; i++) {
      std::string name = ConvertJavaStringToUTF8(env, static_cast<jstring>(
          env->GetObjectArrayElement(projection, i)));
      history::HistoryAndBookmarkRow::ColumnID id =
          history::HistoryAndBookmarkRow::GetColumnID(name);
      if (id == history::HistoryAndBookmarkRow::COLUMN_END) {
        // Ignore the unknown column; As Android platform will send us
        // the non public column.
        continue;
      }
      query_columns.push_back(id);
      columns_name.push_back(name);
    }
  }

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections) {
    where_clause = ConvertJavaStringToUTF8(env, selections);
  }

  std::string sort_clause;
  if (sort_order) {
    sort_clause = ConvertJavaStringToUTF8(env, sort_order);
  }

  QueryBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  history::AndroidStatement* statement = task.Run(
      query_columns, where_clause, where_args, sort_clause);
  if (!statement)
    return ScopedJavaLocalRef<jobject>();

  // Creates and returns org.chromium.chrome.browser.database.SQLiteCursor
  // Java object.
  return SQLiteCursor::NewJavaSqliteCursor(env, columns_name, statement,
             service_.get(), favicon_service_.get());
}

// Updates the bookmarks with the given column values. The value is not given if
// it is NULL.
jint ChromeBrowserProvider::UpdateBookmarkFromAPI(JNIEnv* env,
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
                                                  jobjectArray selection_args) {
  history::HistoryAndBookmarkRow row;
  FillBookmarkRow(env, obj, url, created, isBookmark, date, favicon, title,
                  visits, parent_id, &row, bookmark_model_);

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  UpdateBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(row, where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveBookmarkFromAPI(JNIEnv* env,
                                                  jobject obj,
                                                  jstring selections,
                                                  jobjectArray selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveBookmarksFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveHistoryFromAPI(JNIEnv* env,
                                                 jobject obj,
                                                 jstring selections,
                                                 jobjectArray selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveHistoryFromAPITask task(service_.get(), &cancelable_task_tracker_);
  return task.Run(where_clause, where_args);
}

// Add the search term with the given column values. The value is not given if
// it is NULL.
jlong ChromeBrowserProvider::AddSearchTermFromAPI(JNIEnv* env,
                                                  jobject obj,
                                                  jstring search_term,
                                                  jobject date) {
  DCHECK(search_term);

  history::SearchRow row;
  FillSearchRow(env, obj, search_term, date, &row);

  // URL must be valid.
  if (row.search_term().empty()) {
    LOG(ERROR) << "Search term is empty.";
    return kInvalidContentProviderId;
  }

  AddSearchTermFromAPITask task(service_.get(),
                                &cancelable_task_tracker_,
                                profile_);
  return task.Run(row);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::QuerySearchTermFromAPI(
    JNIEnv* env,
    jobject obj,
    jobjectArray projection,
    jstring selections,
    jobjectArray selection_args,
    jstring sort_order) {
  // Converts the projection to array of ColumnID and column name.
  // Used to store the projection column ID according their sequence.
  std::vector<history::SearchRow::ColumnID> query_columns;
  // Used to store the projection column names according their sequence.
  std::vector<std::string> columns_name;
  if (projection) {
    jsize len = env->GetArrayLength(projection);
    for (int i = 0; i < len; i++) {
      std::string name = ConvertJavaStringToUTF8(env, static_cast<jstring>(
          env->GetObjectArrayElement(projection, i)));
      history::SearchRow::ColumnID id =
          history::SearchRow::GetColumnID(name);
      if (id == history::SearchRow::COLUMN_END) {
        LOG(ERROR) << "Can not find " << name;
        return ScopedJavaLocalRef<jobject>();
      }
      query_columns.push_back(id);
      columns_name.push_back(name);
    }
  }

  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections) {
    where_clause = ConvertJavaStringToUTF8(env, selections);
  }

  std::string sort_clause;
  if (sort_order) {
    sort_clause = ConvertJavaStringToUTF8(env, sort_order);
  }

  QuerySearchTermsFromAPITask task(service_.get(),
                                   &cancelable_task_tracker_,
                                   profile_);
  history::AndroidStatement* statement = task.Run(
      query_columns, where_clause, where_args, sort_clause);
  if (!statement)
    return ScopedJavaLocalRef<jobject>();
  // Creates and returns org.chromium.chrome.browser.database.SQLiteCursor
  // Java object.
  return SQLiteCursor::NewJavaSqliteCursor(env, columns_name, statement,
             service_.get(), favicon_service_.get());
}

// Updates the search terms with the given column values. The value is not
// given if it is NULL.
jint ChromeBrowserProvider::UpdateSearchTermFromAPI(
    JNIEnv* env, jobject obj, jstring search_term, jobject date,
    jstring selections, jobjectArray selection_args) {
  history::SearchRow row;
  FillSearchRow(env, obj, search_term, date, &row);

  std::vector<base::string16> where_args = ConvertJStringArrayToString16Array(
      env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  UpdateSearchTermsFromAPITask task(service_.get(),
                                    &cancelable_task_tracker_,
                                    profile_);
  return task.Run(row, where_clause, where_args);
}

jint ChromeBrowserProvider::RemoveSearchTermFromAPI(
    JNIEnv* env, jobject obj, jstring selections, jobjectArray selection_args) {
  std::vector<base::string16> where_args =
      ConvertJStringArrayToString16Array(env, selection_args);

  std::string where_clause;
  if (selections)
    where_clause = ConvertJavaStringToUTF8(env, selections);

  RemoveSearchTermsFromAPITask task(service_.get(),
                                    &cancelable_task_tracker_,
                                    profile_);
  return task.Run(where_clause, where_args);
}

// ------------- Provider custom APIs ------------- //

jboolean ChromeBrowserProvider::BookmarkNodeExists(
    JNIEnv* env,
    jobject obj,
    jlong id) {
  BookmarkNodeExistsTask task(bookmark_model_);
  return task.Run(id);
}

jlong ChromeBrowserProvider::CreateBookmarksFolderOnce(
    JNIEnv* env,
    jobject obj,
    jstring jtitle,
    jlong parent_id) {
  base::string16 title = ConvertJavaStringToUTF16(env, jtitle);
  if (title.empty())
    return kInvalidBookmarkId;

  CreateBookmarksFolderOnceTask task(bookmark_model_);
  return task.Run(title, parent_id);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::GetEditableBookmarkFolders(
    JNIEnv* env,
    jobject obj) {
  ScopedJavaGlobalRef<jobject> jroot;
  ChromeBookmarkClient* client =
      ChromeBookmarkClientFactory::GetForProfile(profile_);
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
  GetEditableBookmarkFoldersTask task(client, model);
  task.Run(&jroot);
  return ScopedJavaLocalRef<jobject>(jroot);
}

void ChromeBrowserProvider::RemoveAllUserBookmarks(JNIEnv* env, jobject obj) {
  RemoveAllUserBookmarksTask task(bookmark_model_);
  task.Run();
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::GetBookmarkNode(
    JNIEnv* env, jobject obj, jlong id, jboolean get_parent,
    jboolean get_children) {
  ScopedJavaGlobalRef<jobject> jnode;
  GetBookmarkNodeTask task(bookmark_model_);
  task.Run(id, get_parent, get_children, &jnode);
  return ScopedJavaLocalRef<jobject>(jnode);
}

ScopedJavaLocalRef<jobject> ChromeBrowserProvider::GetMobileBookmarksFolder(
    JNIEnv* env,
    jobject obj) {
  ScopedJavaGlobalRef<jobject> jnode;
  GetMobileBookmarksNodeTask task(bookmark_model_);
  ConvertBookmarkNode(task.Run(), ScopedJavaLocalRef<jobject>(), &jnode);
  return ScopedJavaLocalRef<jobject>(jnode);
}

jboolean ChromeBrowserProvider::IsBookmarkInMobileBookmarksBranch(
    JNIEnv* env,
    jobject obj,
    jlong id) {
  IsInMobileBookmarksBranchTask task(bookmark_model_);
  return task.Run(id);
}

ScopedJavaLocalRef<jbyteArray> ChromeBrowserProvider::GetFaviconOrTouchIcon(
    JNIEnv* env, jobject obj, jstring jurl) {
  if (!jurl)
    return ScopedJavaLocalRef<jbyteArray>();

  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));
  BookmarkIconFetchTask favicon_task(
      favicon_service_.get(), &cancelable_task_tracker_, profile_);
  favicon_base::FaviconRawBitmapResult bitmap_result = favicon_task.Run(url);

  if (!bitmap_result.is_valid() || !bitmap_result.bitmap_data.get())
    return ScopedJavaLocalRef<jbyteArray>();

  return base::android::ToJavaByteArray(env, bitmap_result.bitmap_data->front(),
                                        bitmap_result.bitmap_data->size());
}

ScopedJavaLocalRef<jbyteArray> ChromeBrowserProvider::GetThumbnail(
    JNIEnv* env, jobject obj, jstring jurl) {
  if (!jurl)
    return ScopedJavaLocalRef<jbyteArray>();
  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));

  // GetPageThumbnail is synchronous and can be called from any thread.
  scoped_refptr<base::RefCountedMemory> thumbnail;
  if (top_sites_)
    top_sites_->GetPageThumbnail(url, false, &thumbnail);

  if (!thumbnail.get() || !thumbnail->front()) {
    return ScopedJavaLocalRef<jbyteArray>();
  }

  return base::android::ToJavaByteArray(env, thumbnail->front(),
      thumbnail->size());
}

// ------------- Observer-related methods ------------- //

void ChromeBrowserProvider::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  handling_extensive_changes_ = true;
}

void ChromeBrowserProvider::ExtensiveBookmarkChangesEnded(
    BookmarkModel* model) {
  handling_extensive_changes_ = false;
  BookmarkModelChanged();
}

void ChromeBrowserProvider::BookmarkModelChanged() {
  if (handling_extensive_changes_)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (obj.is_null())
    return;

  Java_ChromeBrowserProvider_onBookmarkChanged(env, obj.obj());
}

void ChromeBrowserProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_HISTORY_URL_VISITED ||
      type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
    if (obj.is_null())
      return;
    Java_ChromeBrowserProvider_onHistoryChanged(env, obj.obj());
  } else if (type ==
      chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
    if (obj.is_null())
      return;
    Java_ChromeBrowserProvider_onSearchTermChanged(env, obj.obj());
  }
}
