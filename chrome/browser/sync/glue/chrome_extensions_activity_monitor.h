// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHROME_EXTENSIONS_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_SYNC_GLUE_CHROME_EXTENSIONS_ACTIVITY_MONITOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/util/extensions_activity_monitor.h"

namespace browser_sync {

// Chrome-specific implementation of syncer::ExtensionsActivityMonitor.
//
// As per the requirements of syncer::ExtensionsActivityMonitor, all
// overridden methods are thread-safe, although this class must be
// created and destroyed on the UI thread.
class ChromeExtensionsActivityMonitor
    : public syncer::ExtensionsActivityMonitor,
      public content::NotificationObserver {
 public:
  ChromeExtensionsActivityMonitor();
  virtual ~ChromeExtensionsActivityMonitor();

  // syncer::ExtensionsActivityMonitor implementation.
  virtual void GetAndClearRecords(Records* buffer) OVERRIDE;
  virtual void PutRecords(const Records& records) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Records records_;
  mutable base::Lock records_lock_;

  // Used only on UI loop.
  content::NotificationRegistrar registrar_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHROME_EXTENSIONS_ACTIVITY_MONITOR_H_
