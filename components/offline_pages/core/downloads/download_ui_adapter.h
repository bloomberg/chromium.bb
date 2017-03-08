// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/downloads/download_ui_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "url/gurl.h"

namespace offline_pages {
// C++ side of the UI Adapter. Mimics DownloadManager/Item/History (since we
// share UI with Downloads).
// An instance of this class is owned by OfflinePageModel and is shared between
// UI components if needed. It manages the cache of DownloadUIItems, so after
// initial load the UI components can synchronously pull the whole list or any
// item by its guid.
// The items are exposed to UI layer (consumer of this class) as an observable
// collection of DownloadUIItems. The consumer is supposed to implement
// the DownloadUIAdapter::Observer interface. The creator of the adapter
// also passes in the Delegate that determines which items in the underlying
// OfflinePage backend are to be included (visible) in the collection.
class DownloadUIAdapter : public OfflinePageModel::Observer,
                          public RequestCoordinator::Observer,
                          public base::SupportsUserData::Data {
 public:
  // Delegate, used to customize behavior of this Adapter.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if the page or request with the specified Client Id should
    // be visible in the collection of items exposed by this Adapter. This also
    // indicates if Observers will be notified about changes for the given page.
    virtual bool IsVisibleInUI(const ClientId& client_id) = 0;
    // Sometimes the item should be in the collection but not visible in the UI,
    // temporarily. This is a relatively special case, for example for Last_N
    // snapshots that are only valid while their tab is alive. When the status
    // of temporary visibility changes, the Delegate is supposed to call
    // DownloadUIAdapter::TemporarilyHiddenStatusChanged().
    virtual bool IsTemporarilyHiddenInUI(const ClientId& client_id) = 0;

    // Delegates need a reference to the UI adapter in order to notify it about
    // visibility changes.
    virtual void SetUIAdapter(DownloadUIAdapter* ui_adapter) = 0;
  };

  // Observer, normally implemented by UI or a Bridge.
  class Observer {
   public:
    // Invoked when UI items are loaded. GetAllItems/GetItem can now be used.
    // Must be listened for in order to start getting the items.
    // If the items are already loaded by the time observer is added, this
    // callback will be posted right away.
    virtual void ItemsLoaded() = 0;

    // Invoked when the UI Item was added, usually as a request to download.
    virtual void ItemAdded(const DownloadUIItem& item) = 0;

    // Invoked when the UI Item was updated. Only guid of the item is guaranteed
    // to survive the update, all other fields can change.
    virtual void ItemUpdated(const DownloadUIItem& item) = 0;

    // Invoked when the UI Item was deleted. At this point, only guid remains.
    virtual void ItemDeleted(const std::string& guid) = 0;

   protected:
    virtual ~Observer() = default;
  };

  DownloadUIAdapter(OfflinePageModel* model,
                    RequestCoordinator* coordinator,
                    std::unique_ptr<Delegate> delegate);
  ~DownloadUIAdapter() override;

  static DownloadUIAdapter* FromOfflinePageModel(OfflinePageModel* model);
  static void AttachToOfflinePageModel(DownloadUIAdapter* adapter,
                                       OfflinePageModel* model);

  // This adapter is potentially shared by UI elements, each of which adds
  // itself as an observer.
  // When the last observer is removed, cached list of items is destroyed and
  // next time the initial loading will take longer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns all UI items. The list contains references to items in the cache
  // and has to be used synchronously.
  std::vector<const DownloadUIItem*> GetAllItems() const;
  // May return nullptr if item with specified guid does not exist.
  const DownloadUIItem* GetItem(const std::string& guid) const;

  // Commands from UI. Start async operations, result is observable
  // via Observer or directly by the user (as in 'open').
  void DeleteItem(const std::string& guid);
  int64_t GetOfflineIdByGuid(const std::string& guid) const;

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

  // RequestCoordinator::Observer
  void OnAdded(const SavePageRequest& request) override;
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override;
  void OnNetworkProgress(const SavePageRequest& request,
                         int64_t received_bytes) override;

  // For the DownloadUIAdapter::Delegate, to report the temporary hidden status
  // change.
  void TemporaryHiddenStatusChanged(const ClientId& client_id);

  Delegate* delegate() { return delegate_.get(); }

 private:
  enum class State { NOT_LOADED, LOADING_PAGES, LOADING_REQUESTS, LOADED };

  struct ItemInfo {
    ItemInfo(const OfflinePageItem& page, bool temporarily_hidden);
    ItemInfo(const SavePageRequest& request, bool temporarily_hidden);
    ~ItemInfo();

    std::unique_ptr<DownloadUIItem> ui_item;

    // Additional cached data, not exposed to UI through DownloadUIItem.
    // Indicates if this item wraps the completed page or in-progress request.
    bool is_request;

    // These are shared between pages and requests.
    int64_t offline_id;

    // ClientId is here to support the Delegate that can toggle temporary
    // visibility of the items in the collection.
    ClientId client_id;

    // This item is present in the collection but temporarily hidden from UI.
    // This is useful when unrelated reasons cause the UI item to be excluded
    // (filtered out) from UI. When item becomes temporarily hidden the adapter
    // issues ItemDeleted notification to observers, and ItemAdded when it
    // becomes visible again.
    bool temporarily_hidden;

   private:
    DISALLOW_COPY_AND_ASSIGN(ItemInfo);
  };

  typedef std::map<std::string, std::unique_ptr<ItemInfo>> DownloadUIItems;

  void LoadCache();
  void ClearCache();

  // Task callbacks.
  void OnOfflinePagesLoaded(const MultipleOfflinePageItemResult& pages);
  void OnRequestsLoaded(std::vector<std::unique_ptr<SavePageRequest>> requests);

  void NotifyItemsLoaded(Observer* observer);
  void OnDeletePagesDone(DeletePageResult result);

  void AddItemHelper(std::unique_ptr<ItemInfo> item_info);
  // This function is not re-entrant.  It temporarily sets |deleting_item_|
  // while it runs, so that functions such as |GetOfflineIdByGuid| will work
  // during the |ItemDeleted| callback.
  void DeleteItemHelper(const std::string& guid);

  // Always valid, this class is a member of the model.
  OfflinePageModel* model_;

  // Always valid, a service.
  RequestCoordinator* request_coordinator_;

  // A delegate, supplied at construction.
  std::unique_ptr<Delegate> delegate_;

  State state_;

  // The cache of UI items. The key is DownloadUIItem.guid.
  DownloadUIItems items_;

  std::unique_ptr<ItemInfo> deleting_item_;

  // The observers.
  base::ObserverList<Observer> observers_;
  int observers_count_;

  base::WeakPtrFactory<DownloadUIAdapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUIAdapter);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
