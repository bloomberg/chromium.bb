// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_extensions_activity_monitor.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace browser_sync {

ChromeExtensionsActivityMonitor::ChromeExtensionsActivityMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // It would be nice if we could specify a Source for each specific function
  // we wanted to observe, but the actual function objects are allocated on
  // the fly so there is no reliable object to point to (same problem if we
  // wanted to use the string name).  Thus, we use all sources and filter in
  // Observe.
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
                 content::NotificationService::AllSources());
}

ChromeExtensionsActivityMonitor::~ChromeExtensionsActivityMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ChromeExtensionsActivityMonitor::GetAndClearRecords(Records* buffer) {
  base::AutoLock lock(records_lock_);
  buffer->clear();
  buffer->swap(records_);
}

void ChromeExtensionsActivityMonitor::PutRecords(const Records& records) {
  base::AutoLock lock(records_lock_);
  for (Records::const_iterator i = records.begin(); i != records.end(); ++i) {
    records_[i->first].extension_id = i->second.extension_id;
    records_[i->first].bookmark_write_count += i->second.bookmark_write_count;
  }
}

void ChromeExtensionsActivityMonitor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  base::AutoLock lock(records_lock_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const extensions::Extension* extension =
      content::Source<const extensions::Extension>(source).ptr();
  const extensions::BookmarksFunction* f =
      content::Details<const extensions::BookmarksFunction>(details).ptr();
  if (f->name() == "bookmarks.update" ||
      f->name() == "bookmarks.move" ||
      f->name() == "bookmarks.create" ||
      f->name() == "bookmarks.removeTree" ||
      f->name() == "bookmarks.remove") {
    Record& record = records_[extension->id()];
    record.extension_id = extension->id();
    record.bookmark_write_count++;
  }
}

}  // namespace browser_sync
