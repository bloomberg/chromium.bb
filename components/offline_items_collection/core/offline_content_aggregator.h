// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "url/gurl.h"

namespace offline_items_collection {

struct OfflineItem;

// An implementation of OfflineContentProvider that aggregates multiple other
// providers into a single set of data.  See the OfflineContentProvider header
// for comments on expected behavior of the interface.  This implementation has
// a few caveats:
// - Once all currently registered providers are initialized this provider will
//   trigger OnItemsAvailable on all observers.  Until then the provider will
//   not be initialized.
// - If a provider is added after OnItemsAvailable was sent, it's initialization
//   will act as a notification for OnItemsAdded.  This provider will still be
//   in the initialized state.
// - Calling any modification method on this provider (Open, Update, Delete,
//   etc.) on an OfflineItem belonging to an uninitialized
//   OfflineContentProvider will be queued until that provider is initialized.
//   NOTE: Any actions taken will be propagated to the provider *before* the
//   observers are notified that the provider is initialized.  This is meant to
//   try to guarantee that the data set incorporates the results of those
//   actions.
//
// Routing to the correct provider:
// - Providers must be registered with a unique namespace.  The OfflineItems
//   created by the provider must also be tagged with the same namespace so that
//   actions taken on the OfflineItem can be routed to the correct internal
//   provider.  The namespace must also be consistent across startups.
class OfflineContentAggregator : public OfflineContentProvider,
                                 public OfflineContentProvider::Observer,
                                 public base::SupportsUserData,
                                 public KeyedService {
 public:
  OfflineContentAggregator();
  ~OfflineContentAggregator() override;

  // Registers a provider and associates it with all OfflineItems with
  // |name_space|.  UI actions taken on OfflineItems with |name_space| will be
  // routed to |provider|.  |provider| is expected to only expose OfflineItems
  // with |name_space| set.
  // It is okay to register the same provider with multiple unique namespaces.
  // The class will work as expected with a few caveats.  These are fixable if
  // they are necessary for proper operation.  Contact dtrainor@ if changes to
  // this behavior is needed.
  //   1. Unregistering the first namespace won't remove any pending actions
  //      that are queued for this provider.  That means the provider might
  //      still get actions for the removed namespace once it is done
  //      initializing itself.  This case must be handled by the individual
  //      provider for now.
  //   2. The provider needs to handle calls to GetAllItems properly (not return
  //      any items for a namespace that it didn't register).
  void RegisterProvider(const std::string& name_space,
                        OfflineContentProvider* provider);

  // Removes the OfflineContentProvider associated with |name_space| from this
  // aggregator.
  void UnregisterProvider(const std::string& name_space);

  // OfflineContentProvider implementation.
  bool AreItemsAvailable() override;
  void OpenItem(const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;
  void GetItemById(const ContentId& id, SingleItemCallback callback) override;
  void GetAllItems(MultipleItemCallback callback) override;
  void GetVisualsForItem(const ContentId& id,
                         const VisualsCallback& callback) override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAvailable(OfflineContentProvider* provider) override;
  void OnItemsAdded(const OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item) override;

  // Checks if the underlying OfflineContentProviders are available.  If so,
  // it calls OnItemsAvailable on all observers that haven't yet been notified
  // of this.
  void CheckAndNotifyItemsAvailable();

  // Checks to see if |provider| is initialized.  If so, this flushes any
  // pending actions taken on OfflineItems that belong to |provider|.
  void FlushPendingActionsIfReady(OfflineContentProvider* provider);

  // Checks if |provider| is initialized.  If so, runs |action|, otherwise
  // queues it to run once |provider| triggers that it is ready.
  // NOTE: It is expected that |provider| is the same as the
  // OfflineContentProvider bound in |action|.  The class provides safety checks
  // for that scenario only.
  void RunIfReady(OfflineContentProvider* provider,
                  const base::Closure& action);

  void OnGetAllItemsDone(OfflineContentProvider* provider,
                         const OfflineItemList& items);
  void OnGetItemByIdDone(SingleItemCallback callback,
                         const base::Optional<OfflineItem>& item);
  void NotifyItemsAdded(const OfflineItemList& items);

  // Stores a map of name_space -> OfflineContentProvider.  These
  // OfflineContentProviders are all aggregated by this class and exposed to the
  // consumer as a single list.
  using OfflineProviderMap = std::map<std::string, OfflineContentProvider*>;
  OfflineProviderMap providers_;

  // Stores a map of OfflineContentProvider -> list of closures that represent
  // all actions that need to be taken on the associated OfflineContentProvider
  // when it becomes initialized.
  using CallbackList = std::vector<base::Closure>;
  using PendingActionMap = std::map<OfflineContentProvider*, CallbackList>;
  PendingActionMap pending_actions_;

  // Used by GetAllItems and the corresponding callback.
  std::vector<MultipleItemCallback> multiple_item_get_callbacks_;
  OfflineItemList aggregated_items_;
  std::set<OfflineContentProvider*> pending_providers_;

  // A list of all currently registered observers.
  base::ObserverList<OfflineContentProvider::Observer> observers_;

  // A set of observers that have been notified that this class is initialized.
  // We do not want to notify them of this initialization more than once, so
  // we track them here.
  using ObserverSet = std::set<OfflineContentProvider::Observer*>;
  ObserverSet signaled_observers_;

  // Whether or not this class currently identifies itself as available and has
  // notified the observers.
  bool sent_on_items_available_;

  base::WeakPtrFactory<OfflineContentAggregator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflineContentAggregator);
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_
