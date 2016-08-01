// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
#define COMPONENTS_OFFLINE_PAGE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_

#include <map>
#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/downloads/download_ui_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_types.h"

namespace offline_pages {

// Key is DownloadUIItem.guid.
typedef
    std::map<std::string, std::unique_ptr<DownloadUIItem>> DownloadUIItemsMap;

// C++ side of the UI Adapter. Mimics DownloadManager/Item/History (since we
// share UI with Downloads).
// An instance of this class is owned by OfflinePageModel and is shared between
// UI components if needed. It manages the cache of DownloadUIItems, so after
// initial load the UI components can synchronously pull the whoel list or any
// item by its guid.
class DownloadUIAdapter : public OfflinePageModel::Observer,
                          public base::SupportsUserData::Data {
 public:
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

  explicit DownloadUIAdapter(OfflinePageModel* model);
  ~DownloadUIAdapter() override;

  static DownloadUIAdapter* FromOfflinePageModel(
      OfflinePageModel* offline_page_model);

  // This adapter is potentially shared by UI elements, each of which adds
  // itself as an observer.
  // When the last observer si removed, cached list of items is destroyed and
  // next time the initial loading will take longer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const DownloadUIItemsMap& GetAllItems() const;
  // May return nullptr.
  const DownloadUIItem* GetItem(const std::string& guid) const;

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

 private:
  void LoadCache();
  void ClearCache();

  // Task callbacks.
  void OnOfflinePagesLoaded(const MultipleOfflinePageItemResult& pages);
  void NotifyItemsLoaded(Observer* observer);
  void OnOfflinePagesChanged(const MultipleOfflinePageItemResult& pages);
  bool IsVisibleInUI(const OfflinePageItem& page);

  // Always valid, this class is a member of the model.
  OfflinePageModel* model_;

  bool is_loaded_;

  // The cache of UI items.
  DownloadUIItemsMap items_;

  // The observers.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<DownloadUIAdapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUIAdapter);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
