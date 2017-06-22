// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ALL_DOWNLOAD_ITEM_NOTIFIER_H_
#define CONTENT_PUBLIC_BROWSER_ALL_DOWNLOAD_ITEM_NOTIFIER_H_

#include <set>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"
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

namespace content {

class CONTENT_EXPORT AllDownloadItemNotifier : public DownloadManager::Observer,
                                               public DownloadItem::Observer {
 public:
  // All of the methods take the DownloadManager so that subclasses can observe
  // multiple managers at once and easily distinguish which manager a given item
  // belongs to.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnDownloadCreated(DownloadManager* manager,
                                   DownloadItem* item) {}
    virtual void OnDownloadUpdated(DownloadManager* manager,
                                   DownloadItem* item) {}
    virtual void OnDownloadOpened(DownloadManager* manager,
                                  DownloadItem* item) {}
    virtual void OnDownloadRemoved(DownloadManager* manager,
                                   DownloadItem* item) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  AllDownloadItemNotifier(DownloadManager* manager, Observer* observer);

  ~AllDownloadItemNotifier() override;

  // Returns NULL if the manager has gone down.
  DownloadManager* GetManager() const { return manager_; }

 private:
  // DownloadManager::Observer
  void ManagerGoingDown(DownloadManager* manager) override;
  void OnDownloadCreated(DownloadManager* manager, DownloadItem* item) override;

  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadOpened(DownloadItem* item) override;
  void OnDownloadRemoved(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;

  DownloadManager* manager_;
  AllDownloadItemNotifier::Observer* observer_;
  std::set<DownloadItem*> observing_;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadItemNotifier);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ALL_DOWNLOAD_ITEM_NOTIFIER_H_
