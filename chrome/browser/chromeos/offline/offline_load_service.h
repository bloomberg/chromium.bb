// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_SERVICE_H_
#pragma once

#include <tr1/unordered_set>

#include "base/ref_counted.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

class GURL;
class NavigationController;
class TabContents;
template<class A> class scoped_ptr;

namespace chromeos {

// OfflineLoadService decides whether or not the given url in the tab
// should be loaded when the network is not available. The current
// implementation simply memorize the tab that loads offline page.
// TODO(oshima): Improve the logic so that we can automatically load
// if there is valid cache content and/or it supports offline mode.
class OfflineLoadService
    : public NotificationObserver,
      public base::RefCountedThreadSafe<OfflineLoadService> {
 public:
  // Returns the singleton instance of the offline load service.
  static OfflineLoadService* Get();

  // Returns true if the tab should proceed with loading the page even if
  // it's offline.
  bool ShouldProceed(int process_id, int render_view_id,
                     const GURL& url) const;

  // Record that the user pressed "procced" button for
  // the tab_contents.
  void Proceeded(int proceed_id, int render_view_id, const GURL& url);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<OfflineLoadService>;
  friend class scoped_ptr<OfflineLoadService>;
  friend class OfflineLoadServiceSingleton;

  OfflineLoadService() {}
  virtual ~OfflineLoadService() {}

  // A method invoked in UI thread to remove |tab_contents| from |tabs_|.
  void RemoveTabContents(TabContents* tab_contents);

  // A method invoked in UI thread to register TAB_CLOSED
  // notification.
  void RegisterNotification(
      NavigationController* navigation_controller);

  NotificationRegistrar registrar_;

  // Set of tabs that should proceed
  std::tr1::unordered_set<const TabContents*> tabs_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_SERVICE_H_
