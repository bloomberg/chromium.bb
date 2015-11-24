// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_archiver.h"
#include "components/offline_pages/offline_page_metadata_store.h"

class GURL;
namespace base {
class SequencedTaskRunner;
class Time;
}
namespace bookmarks {
class BookmarkModel;
}

namespace offline_pages {

struct OfflinePageItem;
class OfflinePageMetadataStore;

// Service for saving pages offline, storing the offline copy and metadata, and
// retrieving them upon request.
//
// Example usage:
//   class ArchiverImpl : public OfflinePageArchiver {
//     // This is a class that knows how to create archiver
//     void CreateArchiver(...) override;
//     ...
//   }
//
//   // In code using the OfflinePagesModel to save a page:
//   scoped_ptr<ArchiverImpl> archiver(new ArchiverImpl());
//   // Callback is of type SavePageCallback.
//   model->SavePage(url, archiver.Pass(), callback);
//
// TODO(fgorski): Things to describe:
// * how to cancel requests and what to expect
class OfflinePageModel : public KeyedService,
                         public bookmarks::BaseBookmarkModelObserver {
 public:
  // Result of saving a page offline.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
  enum class SavePageResult {
    SUCCESS,
    CANCELLED,
    DEVICE_FULL,
    CONTENT_UNAVAILABLE,
    ARCHIVE_CREATION_FAILED,
    STORE_FAILURE,
    ALREADY_EXISTS,
    // Certain pages, i.e. file URL or NTP, will not be saved because these
    // are already locally accisible.
    SKIPPED,
    // NOTE: always keep this entry at the end. Add new result types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    RESULT_COUNT,
  };

  // Result of deleting an offline page.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
  enum class DeletePageResult {
    SUCCESS,
    CANCELLED,
    STORE_FAILURE,
    DEVICE_FAILURE,
    NOT_FOUND,
    // NOTE: always keep this entry at the end. Add new result types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    RESULT_COUNT,
  };

  // Result of loading all pages.
  enum class LoadResult {
    SUCCESS,
    CANCELLED,
    STORE_FAILURE,
  };

  // Observer of the OfflinePageModel.
  class Observer {
   public:
    // Invoked when the model has finished loading.
    virtual void OfflinePageModelLoaded(OfflinePageModel* model) = 0;

    // Invoked when the model is being updated, due to adding, removing or
    // updating an offline page.
    virtual void OfflinePageModelChanged(OfflinePageModel* model) = 0;

    // Invoked when an offline copy related to |bookmark_id| was deleted.
    // In can be invoked as a result of |CheckForExternalFileDeletion|, if a
    // deleted page is detected.
    virtual void OfflinePageDeleted(int64 bookmark_id) = 0;

   protected:
    virtual ~Observer() {}
  };

  typedef base::Callback<void(SavePageResult)> SavePageCallback;
  typedef base::Callback<void(DeletePageResult)> DeletePageCallback;

  // Returns true if an offline copy can be saved for the given URL.
  static bool CanSavePage(const GURL& url);

  // All blocking calls/disk access will happen on the provided |task_runner|.
  OfflinePageModel(scoped_ptr<OfflinePageMetadataStore> store,
                   const base::FilePath& archives_dir,
                   const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModel() override;

  // Starts the OfflinePageModel and registers it as a BookmarkModelObserver.
  // Calling this method is optional, but offline pages will not be deleted
  // when the bookmark is deleted, i.e. due to sync, until this method is
  // called.
  void Start(bookmarks::BookmarkModel* model);

  // KeyedService implementation.
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Attempts to save a page addressed by |url| offline. Requires that the model
  // is loaded.
  void SavePage(const GURL& url,
                int64 bookmark_id,
                scoped_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback);

  // Marks that the offline page related to the passed |bookmark_id| has been
  // accessed. Its access info, including last access time and access count,
  // will be updated. Requires that the model is loaded.
  void MarkPageAccessed(int64 bookmark_id);

  // Marks that the offline page related to the passed |bookmark_id| was going
  // to be deleted. The deletion will occur in a short while. The undo can be
  // done before this. Requires that the model is loaded.
  void MarkPageForDeletion(int64 bookmark_id,
                           const DeletePageCallback& callback);

  // Deletes an offline page related to the passed |bookmark_id|. Requires that
  // the model is loaded.
  void DeletePageByBookmarkId(int64 bookmark_id,
                              const DeletePageCallback& callback);

  // Deletes offline pages related to the passed |bookmark_ids|. Requires that
  // the model is loaded.
  void DeletePagesByBookmarkId(const std::vector<int64>& bookmark_ids,
                               const DeletePageCallback& callback);

  // Wipes out all the data by deleting all saved files and clearing the store.
  void ClearAll(const base::Closure& callback);

  // Returns true if there're offline pages.
  bool HasOfflinePages() const;

  // Gets all available offline pages. Requires that the model is loaded.
  const std::vector<OfflinePageItem> GetAllPages() const;

  // Gets pages that should be removed to clean up storage. Requires that the
  // model is loaded.
  const std::vector<OfflinePageItem> GetPagesToCleanUp() const;

  // Returns an offline page associated with a specified |bookmark_id|. nullptr
  // is returned if not found.
  const OfflinePageItem* GetPageByBookmarkId(int64 bookmark_id) const;

  // Returns an offline page that is stored as |offline_url|. A nullptr is
  // returned if not found.
  const OfflinePageItem* GetPageByOfflineURL(const GURL& offline_url) const;

  // Returns an offline page saved for |online_url|. A nullptr is returned if
  // not found.
  const OfflinePageItem* GetPageByOnlineURL(const GURL& online_url) const;

  // Checks that all of the offline pages have corresponding offline copies.
  // If a page is discovered to be missing an offline copy, its offline page
  // metadata will be removed and |OfflinePageDeleted| will be sent to model
  // observers.
  void CheckForExternalFileDeletion();

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

  bool is_loaded() const { return is_loaded_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(OfflinePageModelTest, MarkPageForDeletion);
  FRIEND_TEST_ALL_PREFIXES(OfflinePageModelTest, BookmarkNodeChangesUrl);

  typedef ScopedVector<OfflinePageArchiver> PendingArchivers;

  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;

  // Callback for ensuring archive directory is created.
  void OnEnsureArchivesDirCreatedDone();

  // Callback for loading pages from the offline page metadata store.
  void OnLoadDone(OfflinePageMetadataStore::LoadStatus load_status,
                  const std::vector<OfflinePageItem>& offline_pages);

  // Steps for saving a page offline.
  void OnCreateArchiveDone(const GURL& requested_url,
                           int64 bookmark_id,
                           const base::Time& start_time,
                           const SavePageCallback& callback,
                           OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           int64 file_size);
  void OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                            const SavePageCallback& callback,
                            const OfflinePageItem& offline_page,
                            bool success);
  void InformSavePageDone(const SavePageCallback& callback,
                          SavePageResult result);
  void DeletePendingArchiver(OfflinePageArchiver* archiver);

  // Steps for deleting files and data for an offline page.
  void OnDeleteArchiveFilesDone(
      const std::vector<int64>& bookmark_ids,
      const DeletePageCallback& callback,
      const bool* success);
  void OnRemoveOfflinePagesDone(const std::vector<int64>& bookmark_ids,
                                const DeletePageCallback& callback,
                                bool success);
  void InformDeletePageDone(const DeletePageCallback& callback,
                            DeletePageResult result);

  void OnMarkPageAccesseDone(const OfflinePageItem& offline_page_item,
                             bool success);

  // Steps for marking an offline page for deletion that can be undone.
  void OnMarkPageForDeletionDone(const OfflinePageItem& offline_page_item,
                                 const DeletePageCallback& callback,
                                 bool success);
  void FinalizePageDeletion();

  // Steps for undoing an offline page deletion.
  void UndoPageDeletion(int64 bookmark_id);
  void OnUndoOfflinePageDone(const OfflinePageItem& offline_page, bool success);

  // Callbacks for checking if offline pages are missing archive files.
  void OnFindPagesMissingArchiveFile(
      const std::vector<int64>* pages_missing_archive_file);
  void OnRemoveOfflinePagesMissingArchiveFileDone(
      const std::vector<int64>& bookmark_ids,
      OfflinePageModel::DeletePageResult result);

  // Steps for clearing all.
  void OnRemoveAllFilesDoneForClearAll(const base::Closure& callback,
                                       DeletePageResult result);
  void OnResetStoreDoneForClearAll(const base::Closure& callback, bool success);
  void OnReloadStoreDoneForClearAll(
      const base::Closure& callback,
      OfflinePageMetadataStore::LoadStatus load_status,
      const std::vector<OfflinePageItem>& offline_pages);

  void CacheLoadedData(const std::vector<OfflinePageItem>& offline_pages);

  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  // Location where all of the archive files will be stored.
  base::FilePath archives_dir_;

  // The observers.
  base::ObserverList<Observer> observers_;

  bool is_loaded_;

  // In memory copy of the offline page metadata, keyed by bookmark IDs.
  std::map<int64, OfflinePageItem> offline_pages_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  // Delayed tasks that should be invoked after the loading is done.
  std::vector<base::Closure> delayed_tasks_;

  ScopedObserver<bookmarks::BookmarkModel, bookmarks::BookmarkModelObserver>
      scoped_observer_;

  base::WeakPtrFactory<OfflinePageModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
