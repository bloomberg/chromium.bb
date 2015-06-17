// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_archiver.h"

class GURL;

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
class OfflinePageModel : public KeyedService,
                         public OfflinePageArchiver::Client {
 public:
  // Interface for clients of OfflinePageModel. Methods on the model accepting
  // a Client pointer as a parameter will return their results using one of the
  // methods on the Client interface.
  class Client {
    // Result of deleting an offline page.
    enum DeletePageResult {
      DELETE_PAGE_SUCCESS,
      DELETE_PAGE_CANCELLED,
      DELETE_PAGE_DB_FAILURE,
      DELETE_PAGE_DOES_NOT_EXIST,
    };

    // Result of loading all pages.
    enum LoadResult {
      LOAD_SUCCESS,
      LOAD_CANCELLED,
      LOAD_DB_FAILURE,
    };

    // Result of saving a page offline.
    enum SavePageResult {
      SAVE_PAGE_SUCCESS,
      SAVE_PAGE_CANCELLED,
      SAVE_PAGE_DB_FAILURE,
      SAVE_PAGE_ALREADY_EXISTS,
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
        const std::vector<OfflinePageItem>& offline_pages) = 0;
  };

  OfflinePageModel(scoped_ptr<OfflinePageMetadataStore> store,
                   OfflinePageArchiver* archiver);
  ~OfflinePageModel() override;

  // KeyedService implementation.
  void Shutdown() override;

  // OfflinePageArchiver::Client implementation.
  void OnCreateArchiveDone(OfflinePageArchiver::Request* request,
                           OfflinePageArchiver::ArchiverResult error,
                           const base::FilePath& file_path) override;

  // Attempts to save a page addressed by |url| offline.
  void SavePage(const GURL& url, Client* client);

  // Deletes an offline page related to the passed |url|.
  void DeletePage(const GURL& url, Client* client);

  // Loads all of the available offline pages.
  void LoadAllPages(Client* client);

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

 private:
  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  // Offline page archiver. Outlives the model. Owned by the embedder.
  OfflinePageArchiver* archiver_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
