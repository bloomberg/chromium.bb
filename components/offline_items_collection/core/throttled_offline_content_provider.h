// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace offline_items_collection {

// A simple wrapper around an OfflineContentProvider that throttles
// OfflineContentProvider::Observer::OnItemUpdated() calls to all registered
// observers.  This class will coalesce updates to an item with an equal
// ContentId.
class ThrottledOfflineContentProvider
    : public OfflineContentProvider,
      public OfflineContentProvider::Observer {
 public:
  explicit ThrottledOfflineContentProvider(OfflineContentProvider* provider);
  ThrottledOfflineContentProvider(const base::TimeDelta& delay_between_updates,
                                  OfflineContentProvider* provider);
  ~ThrottledOfflineContentProvider() override;

  // OfflineContentProvider implementation.
  bool AreItemsAvailable() override;
  void OpenItem(const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id) override;

  // Because this class queues updates, a call to Observer::OnItemUpdated might
  // get triggered with the same contents as returned by these getter methods in
  // the future.
  const OfflineItem* GetItemById(const ContentId& id) override;
  OfflineItemList GetAllItems() override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAvailable(OfflineContentProvider* provider) override;
  void OnItemsAdded(const OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item) override;

  // Used to notify |observer| that the underying OfflineContentProvider has
  // called OfflineContentProvider::Observer::OnItemsAvailable().
  void NotifyItemsAvailable(OfflineContentProvider::Observer* observer);

  // Checks if |item| already has an update pending.  If so, replaces the
  // content of the update with |item|.
  void UpdateItemIfPresent(const OfflineItem& item);

  // Flushes all pending updates to the observers.
  void FlushUpdates();

  const base::TimeDelta delay_between_updates_;
  bool update_queued_;

  OfflineContentProvider* const wrapped_provider_;
  base::ObserverList<OfflineContentProvider::Observer> observers_;

  typedef std::map<ContentId, OfflineItem> OfflineItemMap;
  OfflineItemMap updates_;

  base::WeakPtrFactory<ThrottledOfflineContentProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottledOfflineContentProvider);
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
