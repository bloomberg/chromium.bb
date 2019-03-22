// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_ALL_DOWNLOAD_ITEM_NOTIFIER_H_
#define COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_ALL_DOWNLOAD_ITEM_NOTIFIER_H_

#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/download/public/common/download_item.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/download_manager.h"

// AllDownloadItemNotifier observes ALL the DownloadItems on a given
// DownloadManager.
// Clients should use GetManager() instead of storing their own pointer to the
// manager so that they can be sensitive to managers that have gone down.

// Example Usage:
// class DownloadSystemConsumer : public AllDownloadItemNotifier::Observer {
//  public:
//   DownloadSystemConsumer(DownloadManager* original_manager,
//            DownloadManager* incognito_manager)
//     : original_notifier_(original_manager, this),
//       incognito_notifier_(incognito_manager, this) {
//   }
//
//   virtual void OnDownloadUpdated(
//     DownloadManager* manager, DownloadItem* item) { ... }
//
//  private:
//   AllDownloadItemNotifier original_notifier_;
//   AllDownloadItemNotifier incognito_notifier_;
// };

namespace download {

class AllDownloadItemNotifier : public content::DownloadManager::Observer,
                                public DownloadItem::Observer,
                                public KeyedService {
 public:
  // All of the methods take the DownloadManager so that subclasses can observe
  // multiple managers at once and easily distinguish which manager a given item
  // belongs to.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnManagerInitialized(content::DownloadManager* manager) {}
    virtual void OnManagerGoingDown(content::DownloadManager* manager) {}
    virtual void OnDownloadCreated(content::DownloadManager* manager,
                                   download::DownloadItem* item) {}
    virtual void OnDownloadUpdated(content::DownloadManager* manager,
                                   download::DownloadItem* item) {}
    virtual void OnDownloadOpened(content::DownloadManager* manager,
                                  download::DownloadItem* item) {}
    virtual void OnDownloadRemoved(content::DownloadManager* manager,
                                   download::DownloadItem* item) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  AllDownloadItemNotifier(content::DownloadManager* manager);

  ~AllDownloadItemNotifier() override;

  // Returns NULL if the manager has gone down.
  content::DownloadManager* GetManager() const { return manager_; }

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Observers implementing the AllDownloadItemNotifier::Observer interface can
  // register here to get notifications of changes to request state.  This
  // pointer is not owned, and it is the callers responsibility to remove the
  // observer before the observer is deleted.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // DownloadManager::Observer
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadOpened(DownloadItem* item) override;
  void OnDownloadRemoved(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;

 private:
  content::DownloadManager* manager_;
  base::ObserverList<Observer>::Unchecked observers_;

  std::set<DownloadItem*> observing_;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadItemNotifier);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_ALL_DOWNLOAD_ITEM_NOTIFIER_H_
