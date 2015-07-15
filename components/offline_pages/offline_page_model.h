// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_archiver.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

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
//   class ModelClient : public OfflinePageModel::Client {
//     ...
//     void OnSavePageDone(SavePageResult result) override {
//       // handles errors or completes the save.
//     }
//     const GURL& url() const { return url_; }
//    private:
//     GURL url_;
//   };
//
//   scoped_ptr<ModelClient> client(new ModelClient());
//   model->SavePage(client->url(), client);
//
// TODO(fgorski): Things to describe:
// * how to cancel requests and what to expect
class OfflinePageModel : public KeyedService {
 public:
  // Interface for clients of OfflinePageModel. Methods on the model accepting
  // a Client pointer as a parameter will return their results using one of the
  // methods on the Client interface.
  class Client {
   public:
    // Result of deleting an offline page.
    enum class DeletePageResult {
      DELETE_PAGE_SUCCESS,
      DELETE_PAGE_CANCELLED,
      DELETE_PAGE_DB_FAILURE,
      DELETE_PAGE_DOES_NOT_EXIST,
    };

    // Result of loading all pages.
    enum class LoadResult {
      LOAD_SUCCESS,
      LOAD_CANCELLED,
      LOAD_DB_FAILURE,
    };

    // Result of saving a page offline.
    enum class SavePageResult {
      SUCCESS,
      CANCELLED,
      DEVICE_FULL,
      CONTENT_UNAVAILABLE,
      ARCHIVE_CREATION_FAILED,
      DB_FAILURE,
      ALREADY_EXISTS,
    };

    virtual ~Client() {}

    // Callback to SavePage call.
    // TODO(fgorski): Should we return a copy of the record or depend on the
    // client to call |LoadAllPages| to see things refreshed?
    virtual void OnSavePageDone(SavePageResult result) = 0;

    // Callback to DeletePage call.
    virtual void OnDeletePageDone(DeletePageResult result) = 0;

    // Callback to LoadAllPages call.
    virtual void OnLoadAllPagesDone(
        LoadResult result,
        std::vector<OfflinePageItem>* offline_pages) = 0;
  };

  OfflinePageModel(
      scoped_ptr<OfflinePageMetadataStore> store,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~OfflinePageModel() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Attempts to save a page addressed by |url| offline.
  void SavePage(const GURL& url,
                scoped_ptr<OfflinePageArchiver> archiver,
                const base::WeakPtr<Client>& client);

  // Deletes an offline page related to the passed |url|.
  void DeletePage(const GURL& url, Client* client);

  // Loads all of the available offline pages.
  void LoadAllPages(Client* client);

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

 private:
  typedef ScopedVector<OfflinePageArchiver> PendingArchivers;

  // OfflinePageArchiver callback.
  void OnCreateArchiveDone(const GURL& requested_url,
                           const base::WeakPtr<Client>& client,
                           OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::string16& title,
                           const base::FilePath& file_path,
                           int64 file_size);

  // OfflinePageMetadataStore callbacks.
  void OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                            const base::WeakPtr<Client>& client,
                            bool success);

  void InformSavePageDone(const base::WeakPtr<Client>& client,
                          Client::SavePageResult result);

  void DeletePendingArchiver(OfflinePageArchiver* archiver);

  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<OfflinePageModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
