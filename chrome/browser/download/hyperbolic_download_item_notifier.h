// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_HYPERBOLIC_DOWNLOAD_ITEM_NOTIFIER_H_
#define CHROME_BROWSER_DOWNLOAD_HYPERBOLIC_DOWNLOAD_ITEM_NOTIFIER_H_

#include <set>

#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_item.h"

// HyperbolicDownloadItemNotifier observes ALL the DownloadItems on a given
// DownloadManager.
// Clients should use GetManager() instead of storing their own pointer to the
// manager so that they can be sensitive to managers that have gone down.

// Example Usage:
// class AndAHalf : public HyperbolicDownloadItemNotifier::Observer {
//  public:
//   AndAHalf(content::DownloadManager* original_manager,
//            content::DownloadManager* incognito_manager)
//     : ALLOW_THIS_IN_INITIALIZATION_LIST(original_hyperbole_(
//           original_manager, this)),
//       ALLOW_THIS_IN_INITIALIZATION_LIST(incognito_hyperbole_(
//           incognito_manager, this)) {
//   }
//
//   virtual void OnDownloadUpdated(
//     content::DownloadManager* manager, content::DownloadItem* item) { ... }
//
//  private:
//   HyperbolicDownloadItemNotifier original_hyperbole_;
//   HyperbolicDownloadItemNotifier incognito_hyperbole_;
// };

class HyperbolicDownloadItemNotifier
  : public content::DownloadManager::Observer,
    public content::DownloadItem::Observer {
 public:
  // All of the methods take the DownloadManager so that subclasses can observe
  // multiple managers at once and easily distinguish which manager a given item
  // belongs to.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnDownloadCreated(
        content::DownloadManager* manager, content::DownloadItem* item) {}
    virtual void OnDownloadUpdated(
        content::DownloadManager* manager, content::DownloadItem* item) {}
    virtual void OnDownloadOpened(
        content::DownloadManager* manager, content::DownloadItem* item) {}
    virtual void OnDownloadRemoved(
        content::DownloadManager* manager, content::DownloadItem* item) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  HyperbolicDownloadItemNotifier(content::DownloadManager* manager,
                                 Observer* observer);

  virtual ~HyperbolicDownloadItemNotifier();

  // Returns NULL if the manager has gone down.
  content::DownloadManager* GetManager() const { return manager_; }

 private:
  // content::DownloadManager::Observer
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;
  virtual void OnDownloadCreated(content::DownloadManager* manager,
                                content::DownloadItem* item) OVERRIDE;

  // content::DownloadItem::Observer
  virtual void OnDownloadUpdated(content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadRemoved(content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* item) OVERRIDE;

  content::DownloadManager* manager_;
  HyperbolicDownloadItemNotifier::Observer* observer_;
  std::set<content::DownloadItem*> observing_;

  DISALLOW_COPY_AND_ASSIGN(HyperbolicDownloadItemNotifier);
};

#endif  // CHROME_BROWSER_DOWNLOAD_HYPERBOLIC_DOWNLOAD_ITEM_NOTIFIER_H_
