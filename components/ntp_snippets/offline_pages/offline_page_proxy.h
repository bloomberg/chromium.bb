// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_PROXY_OFFLINE_PAGE_PROXY_H_
#define COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_PROXY_OFFLINE_PAGE_PROXY_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_types.h"

namespace ntp_snippets {

// Observes offline pages model and propagates updates to its observers.
class OfflinePageProxy : public offline_pages::OfflinePageModel::Observer,
                         public base::RefCounted<OfflinePageProxy> {
 public:
  class Observer {
   public:
    // Corresponds to OfflinePageModel::Observer::OfflinePageModelChanged.
    // Invoked when the model is being updated, due to adding, removing or
    // updating an offline page. |offline_pages| contains all offline pages
    // after the update.
    virtual void OfflinePageModelChanged(
        const std::vector<offline_pages::OfflinePageItem>& offline_pages) = 0;

    // Corresponds to OfflinePageModel::Observer::OfflinePageDeleted.
    // Invoked when an offline copy related to |offline_id| was deleted.
    virtual void OfflinePageDeleted(
        int64_t offline_id,
        const offline_pages::ClientId& client_id) = 0;

   protected:
    virtual ~Observer() = default;
  };

  explicit OfflinePageProxy(
      offline_pages::OfflinePageModel* offline_page_model);

  // TODO(vitaliii): Remove this function and provide a better way for providers
  // to get data at the start up, while querying OfflinePagesModel only once.
  // Queries OfflinePageModel for all pages and returns them through |callback|.
  void GetAllPages(
      const offline_pages::MultipleOfflinePageItemCallback& callback);

  // Observer accessors.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCounted<OfflinePageProxy>;

  ~OfflinePageProxy() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageModelChanged(offline_pages::OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const offline_pages::ClientId& client_id) override;

  // Queries the OfflinePageModel for offline pages and notifies observers
  // through |OfflinePageModelChanged|.
  void FetchOfflinePagesAndNotify();

  // Callback from the |OfflinePageModel::GetAllPages|.
  void OnOfflinePagesLoaded(
      const offline_pages::MultipleOfflinePageItemResult& result);

  offline_pages::OfflinePageModel* offline_page_model_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<OfflinePageProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageProxy);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_PROXY_OFFLINE_PAGE_PROXY_H_
