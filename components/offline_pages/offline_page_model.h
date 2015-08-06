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
// Caller of |SavePage|, |DeletePage| and |LoadAllPages| should provide
// implementation of |Client|, which will be then used to return a result of
// respective calls.
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

  typedef base::Callback<void(SavePageResult)> SavePageCallback;
  typedef base::Callback<void(DeletePageResult)> DeletePageCallback;
  typedef base::Callback<void(LoadResult, const std::vector<OfflinePageItem>&)>
      LoadAllPagesCallback;

  // All blocking calls/disk access will happen on the provided |task_runner|.
  OfflinePageModel(
      scoped_ptr<OfflinePageMetadataStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModel() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Attempts to save a page addressed by |url| offline.
  void SavePage(const GURL& url,
                int64 bookmark_id,
                scoped_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback);

  // Deletes an offline page related to the passed |url|.
  void DeletePage(const GURL& url, const DeletePageCallback& callback);

  // Loads all of the available offline pages.
  void LoadAllPages(const LoadAllPagesCallback& callback);

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

 private:
  typedef ScopedVector<OfflinePageArchiver> PendingArchivers;

  // OfflinePageArchiver callback.
  void OnCreateArchiveDone(const GURL& requested_url,
                           int64 bookmark_id,
                           const SavePageCallback& callback,
                           OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           int64 file_size);

  // OfflinePageMetadataStore callbacks.
  void OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                            const SavePageCallback& callback,
                            bool success);
  void OnLoadDone(const LoadAllPagesCallback& callback,
                  bool success,
                  const std::vector<OfflinePageItem>& offline_pages);

  void InformSavePageDone(const SavePageCallback& callback,
                          SavePageResult result);

  void DeletePendingArchiver(OfflinePageArchiver* archiver);

  // Serialized steps of deleting files and data for an offline page.
  void OnLoadDoneForDeletion(const GURL& url,
                             const DeletePageCallback& callback,
                             bool success,
                             const std::vector<OfflinePageItem>& offline_pages);
  void DeleteArchiverFile(const base::FilePath& file_path, bool* success);
  void OnDeleteArchiverFileDone(
      const GURL& url,
      const DeletePageCallback& callback,
      const bool* success);
  void OnRemoveOfflinePageDone(
      const DeletePageCallback& callback, bool success);

  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  base::WeakPtrFactory<OfflinePageModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
