// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace offline_items_collection {

struct ContentId;
struct OfflineItem;
struct OfflineItemVisuals;

// A provider of a set of OfflineItems that are meant to be exposed to the UI.
// The provider is required to notify all observers of OnItemsAvailable when the
// underlying data set is initialized.  Without that call it should not expect
// nor have to support any other calls to this provider.
class OfflineContentProvider {
 public:
  using OfflineItemList = std::vector<OfflineItem>;
  using VisualsCallback =
      base::Callback<void(const ContentId&, const OfflineItemVisuals*)>;
  using MultipleItemCallback = base::OnceCallback<void(const OfflineItemList&)>;
  using SingleItemCallback =
      base::OnceCallback<void(const base::Optional<OfflineItem>&)>;

  // An observer class that should be notified of relevant changes to the
  // underlying data source.
  class Observer {
   public:
    // Called when the underlying data source for the provider has been
    // initialized and the contents are able to be queried and interacted with.
    // |provider| should be a reference to this OfflineContentProvider that is
    // initialized.
    virtual void OnItemsAvailable(OfflineContentProvider* provider) = 0;

    // Called when one or more OfflineItems have been added and should be shown
    // in the UI.
    virtual void OnItemsAdded(const OfflineItemList& items) = 0;

    // Called when the OfflineItem represented by |id| should be removed from
    // the UI.
    virtual void OnItemRemoved(const ContentId& id) = 0;

    // Called when the contents of |item| have been updated and the UI should be
    // refreshed for that item.
    // TODO(dtrainor): Make this take a list of OfflineItems.
    virtual void OnItemUpdated(const OfflineItem& item) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Returns whether or not the underlying data source for this provider has
  // been initialized and is ready to start returning content.  This provider
  // should not need to support handling the other data query/manipulation
  // methods if this returns false.
  virtual bool AreItemsAvailable() = 0;

  // Called to trigger opening an OfflineItem represented by |id|.
  virtual void OpenItem(const ContentId& id) = 0;

  // Called to trigger removal of an OfflineItem represented by |id|.
  virtual void RemoveItem(const ContentId& id) = 0;

  // Called to cancel a download of an OfflineItem represented by |id|.
  virtual void CancelDownload(const ContentId& id) = 0;

  // Called to pause a download of an OfflineItem represented by |id|.
  virtual void PauseDownload(const ContentId& id) = 0;

  // Called to resume a paused download of an OfflineItem represented by |id|.
  // TODO(shaktisahu): Remove |has_user_gesture| if we end up not needing it.
  virtual void ResumeDownload(const ContentId& id, bool has_user_gesture) = 0;

  // Requests for an OfflineItem represented by |id|. The implementer should
  // post any replies even if the result is available immediately to prevent
  // reentrancy and for consistent behavior.
  virtual void GetItemById(const ContentId& id,
                           SingleItemCallback callback) = 0;

  // Requests for all the OfflineItems from this particular provider. The
  // implementer should post any replies even if the results are available
  // immediately to prevent reentrancy and for consistent behavior.
  virtual void GetAllItems(MultipleItemCallback callback) = 0;

  // Asks for an OfflineItemVisuals struct for an OfflineItem represented by
  // |id| or |nullptr| if one doesn't exist.  The implementer should post any
  // replies even if the results are available immediately to prevent reentrancy
  // and for consistent behavior.
  virtual void GetVisualsForItem(const ContentId& id,
                                 const VisualsCallback& callback) = 0;

  // Adds an observer that should be notified of OfflineItem list modifications.
  // If the provider is already initialized OnItemsAvailable should be scheduled
  // on this observer (suggested over calling the method directly to avoid
  // reentrancy).
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.  No further notifications should be sent to it.
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~OfflineContentProvider() = default;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_OFFLINE_CONTENT_PROVIDER_H_
