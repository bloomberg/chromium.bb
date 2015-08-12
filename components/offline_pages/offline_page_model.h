// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_archiver.h"

class GURL;
namespace base {
class SequencedTaskRunner;
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
class OfflinePageModel : public KeyedService {
 public:
  // Result of saving a page offline.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_pages
  enum class SavePageResult {
    SUCCESS,
    CANCELLED,
    DEVICE_FULL,
    CONTENT_UNAVAILABLE,
    ARCHIVE_CREATION_FAILED,
    STORE_FAILURE,
    ALREADY_EXISTS,
  };

  // Result of deleting an offline page.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_pages
  enum class DeletePageResult {
    SUCCESS,
    CANCELLED,
    STORE_FAILURE,
    DEVICE_FAILURE,
    NOT_FOUND,
  };

  // Result of loading all pages.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_pages
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

   protected:
    virtual ~Observer() {}
  };

  typedef base::Callback<void(SavePageResult)> SavePageCallback;
  typedef base::Callback<void(DeletePageResult)> DeletePageCallback;

  // All blocking calls/disk access will happen on the provided |task_runner|.
  OfflinePageModel(
      scoped_ptr<OfflinePageMetadataStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModel() override;

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

  // Deletes an offline page related to the passed |bookmark_id|. Requires that
  // the model is loaded.
  void DeletePageByBookmarkId(int64 bookmark_id,
                              const DeletePageCallback& callback);

  // Gets all available offline pages. Requires that the model is loaded.
  const std::vector<OfflinePageItem>& GetAllPages() const;

  // Gets an offline page associated with a specified |bookmark_id|. Returns
  // true if a matching offline page exists, and |offline_page| will be updated
  // with corresponding value, or false, if no offline page was found.
  bool GetPageByBookmarkId(int64 bookmark_id,
                           OfflinePageItem* offline_page) const;

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

  bool is_loaded() const { return is_loaded_; }

 private:
  typedef ScopedVector<OfflinePageArchiver> PendingArchivers;

  void DeletePage(const OfflinePageItem& offline_page,
                  const  DeletePageCallback& callback);

  // Callback for loading pages from the offline page metadata store.
  void OnLoadDone(bool success,
                  const std::vector<OfflinePageItem>& offline_pages);

  // Steps for saving a page offline.
  void OnCreateArchiveDone(const GURL& requested_url,
                           int64 bookmark_id,
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
  void OnDeleteArchiverFileDone(
      const GURL& url,
      const DeletePageCallback& callback,
      const bool* success);
  void OnRemoveOfflinePageDone(const GURL& url,
                               const DeletePageCallback& callback,
                               bool success);

  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  // The observers.
  base::ObserverList<Observer> observers_;

  bool is_loaded_;

  // In memory copy of the offline page metadata.
  std::vector<OfflinePageItem> offline_pages_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  base::WeakPtrFactory<OfflinePageModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
