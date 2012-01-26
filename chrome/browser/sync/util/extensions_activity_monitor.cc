// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/extensions_activity_monitor.h"

#include "base/bind.h"
#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionsActivityMonitor::ExtensionsActivityMonitor() {
  // In normal use the ExtensionsActivityMonitor is deleted by a message posted
  // to the UI thread, so this task will run before |this| goes out of scope
  // and ref-counting isn't needed; hence Unretained(this).
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionsActivityMonitor::RegisterNotificationsOnUIThread,
                 base::Unretained(this)));
}

ExtensionsActivityMonitor::~ExtensionsActivityMonitor() {
  // In some unrelated unit tests, there is no running UI loop.  In this case,
  // the PostTask in our ctor will not result in anything running, so |this|
  // won't be used for anything. In this case (or whenever no registration took
  // place) and only this case we allow destruction on another loop, but this
  // isn't something a client of this class can control; it happens implicitly
  // by not having a running UI thread.
  if (registrar_.get()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // The registrar calls RemoveAll in its dtor (which would happen in a
    // moment anyway) but explicitly destroy it so it is clear why we need to
    // be on the UI thread.
    registrar_.reset();
  }
}

void ExtensionsActivityMonitor::GetAndClearRecords(Records* buffer) {
  base::AutoLock lock(records_lock_);
  buffer->clear();
  buffer->swap(records_);
}

void ExtensionsActivityMonitor::PutRecords(const Records& records) {
  base::AutoLock lock(records_lock_);
  for (Records::const_iterator i = records.begin(); i != records.end(); ++i) {
    records_[i->first].extension_id = i->second.extension_id;
    records_[i->first].bookmark_write_count += i->second.bookmark_write_count;
  }
}

void ExtensionsActivityMonitor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  base::AutoLock lock(records_lock_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const Extension* extension = content::Source<const Extension>(source).ptr();
  const BookmarksFunction* f =
      content::Details<const BookmarksFunction>(details).ptr();
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

void ExtensionsActivityMonitor::RegisterNotificationsOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // It would be nice if we could specify a Source for each specific function
  // we wanted to observe, but the actual function objects are allocated on
  // the fly so there is no reliable object to point to (same problem if we
  // wanted to use the string name).  Thus, we use all sources and filter in
  // Observe.
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this,
                  chrome::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
                  content::NotificationService::AllSources());
}

}  // namespace browser_sync
