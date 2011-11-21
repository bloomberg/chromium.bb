// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser_sync {

// A class to monitor usage of extensions APIs to send to sync servers, with
// the ability to purge data once sync servers have acknowledged it (successful
// commit response).
//
// This can be used from any thread (it is a 'monitor' in the synchronization
// sense as well), HOWEVER
//
// *** IT MUST BE DELETED FROM THE UI LOOP ***
//
// Consider using MessageLoop::DeleteSoon. (Yes, this means if you allocate
// an ExtensionsActivityMonitor on a thread other than UI, you must 'new' it).
class ExtensionsActivityMonitor : public content::NotificationObserver {
 public:
  // A data record of activity performed by extension |extension_id|.
  struct Record {
    Record() : bookmark_write_count(0U) {}

    // The human-readable ID identifying the extension responsible
    // for the activity reported in this Record.
    std::string extension_id;

    // How many times the extension successfully invoked a write
    // operation through the bookmarks API since the last CommitMessage.
    uint32 bookmark_write_count;
  };

  typedef std::map<std::string, Record> Records;

  // Creates an ExtensionsActivityMonitor to monitor extensions activities on
  // BrowserThread::UI.
  ExtensionsActivityMonitor();
  virtual ~ExtensionsActivityMonitor();

  // Fills |buffer| with snapshot of current records in constant time by
  // swapping.  This is done mutually exclusively w.r.t methods of this class.
  void GetAndClearRecords(Records* buffer);

  // Add |records| piece-wise (by extension id) to the set of active records.
  // This is done mutually exclusively w.r.t the methods of this class.
  void PutRecords(const Records& records);

  // content::NotificationObserver implementation.  Called on |ui_loop_|.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  Records records_;
  mutable base::Lock records_lock_;

  // Used only from UI loop.
  content::NotificationRegistrar registrar_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_
