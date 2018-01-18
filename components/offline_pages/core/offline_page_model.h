// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_model_query.h"
#include "components/offline_pages/core/offline_page_storage_manager.h"
#include "components/offline_pages/core/offline_page_types.h"

class GURL;

namespace offline_pages {

struct ClientId;

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
//   std::unique_ptr<ArchiverImpl> archiver(new ArchiverImpl());
//   // Callback is of type SavePageCallback.
//   model->SavePage(url, std::move(archiver), callback);
//
// TODO(fgorski): Things to describe:
// * how to cancel requests and what to expect
class OfflinePageModel : public base::SupportsUserData, public KeyedService {
 public:
  // Describes the parameters to control how to save a page.
  struct SavePageParams {
    SavePageParams();
    SavePageParams(const SavePageParams& other);
    ~SavePageParams();

    // The last committed URL of the page to save.
    GURL url;

    // The identification used by the client.
    ClientId client_id;

    // Used for the offline_id for the saved file if non-zero. If it is
    // kInvalidOfflineId, a new, random ID will be generated.
    int64_t proposed_offline_id;

    // The original URL of the page to save. Empty if no redirect occurs.
    GURL original_url;

    // Whether the page is being saved in the background.
    bool is_background;

    // Run page problem detectors while generating MTHML if true.
    bool use_page_problem_detectors;

    // The app package that the request originated from.
    std::string request_origin;
  };

  // Information about a deleted page.
  struct DeletedPageInfo {
    DeletedPageInfo();
    DeletedPageInfo(const DeletedPageInfo& other);
    ~DeletedPageInfo();
    DeletedPageInfo(int64_t offline_id,
                    const ClientId& client_id,
                    const std::string& request_origin);
    // The ID of the deleted page.
    int64_t offline_id;
    // Client ID of the deleted page.
    ClientId client_id;
    // The origin that the page was saved on behalf of.
    std::string request_origin;
  };

  // Observer of the OfflinePageModel.
  class Observer {
   public:
    // Invoked when the model has finished loading.
    virtual void OfflinePageModelLoaded(OfflinePageModel* model) = 0;

    // Invoked when the model is being updated due to adding an offline page.
    virtual void OfflinePageAdded(OfflinePageModel* model,
                                  const OfflinePageItem& added_page) = 0;

    // Invoked when an offline copy related to |offline_id| was deleted.
    virtual void OfflinePageDeleted(const DeletedPageInfo& page_info) = 0;

   protected:
    virtual ~Observer() = default;
  };

  using MultipleOfflinePageItemResult =
      offline_pages::MultipleOfflinePageItemResult;
  using DeletePageResult = offline_pages::DeletePageResult;
  using SavePageResult = offline_pages::SavePageResult;

  // Returns true if saving an offline page may be attempted for |url|.
  static bool CanSaveURL(const GURL& url);

  OfflinePageModel();
  ~OfflinePageModel() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  static const int64_t kInvalidOfflineId = 0;

  // Attempts to save a page offline per |save_page_params|. Requires that the
  // model is loaded.  Generates a new offline id or uses the proposed offline
  // id in |save_page_params| and returns it.
  virtual void SavePage(const SavePageParams& save_page_params,
                        std::unique_ptr<OfflinePageArchiver> archiver,
                        const SavePageCallback& callback) = 0;

  // Adds a page entry to the metadata store.
  virtual void AddPage(const OfflinePageItem& page,
                       const AddPageCallback& callback) = 0;

  // Marks that the offline page related to the passed |offline_id| has been
  // accessed. Its access info, including last access time and access count,
  // will be updated. Requires that the model is loaded.
  virtual void MarkPageAccessed(int64_t offline_id) = 0;

  // Deletes pages based on |offline_ids|.
  virtual void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                      const DeletePageCallback& callback) = 0;

  // Deletes all pages associated with any of |client_ids|.
  virtual void DeletePagesByClientIds(const std::vector<ClientId>& client_ids,
                                      const DeletePageCallback& callback) = 0;

  // Deletes cached offline pages matching the URL predicate.
  virtual void DeleteCachedPagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) = 0;

  // Gets all offline pages.
  virtual void GetAllPages(const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns zero or one offline pages associated with a specified |offline_id|.
  virtual void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) = 0;

  // Retrieves all pages associated with any of |client_ids|.
  virtual void GetPagesByClientIds(
      const std::vector<ClientId>& client_ids,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns the offline pages that are related to |url|. |url_search_mode|
  // controls how the url match is done. See URLSearchMode for more details.
  virtual void GetPagesByURL(
      const GURL& url,
      URLSearchMode url_search_mode,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns the offline pages that belong in |name_space|.
  virtual void GetPagesByNamespace(
      const std::string& name_space,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns the offline pages that are removed when cache is reset.
  virtual void GetPagesRemovedOnCacheReset(
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns the offline pages that are visible in download manager UI.
  virtual void GetPagesSupportedByDownloads(
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Retrieves all pages associated with the |request_origin|.
  virtual void GetPagesByRequestOrigin(
      const std::string& request_origin,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Gets all offline ids where the offline page has the matching client id.
  virtual void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) = 0;

  // Returns the policy controller.
  virtual ClientPolicyController* GetPolicyController() = 0;

  // Get the archive directory based on client policy of the namespace.
  virtual const base::FilePath& GetInternalArchiveDirectory(
      const std::string& name_space) const = 0;

  // Returns whether given archive file is in the internal directory.
  virtual bool IsArchiveInInternalDir(
      const base::FilePath& file_path) const = 0;

  // Returns the logger. Ownership is retained by the model.
  virtual OfflineEventLogger* GetLogger() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_H_
