// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/extensions_activity_monitor.h"

#include "base/task.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

namespace {
// A helper task to register an ExtensionsActivityMonitor as an observer of
// events on the UI thread (even though the monitor may live on another thread).
// This liberates ExtensionsActivityMonitor from having to be ref counted.
class RegistrationTask : public Task {
 public:
  RegistrationTask(ExtensionsActivityMonitor* monitor,
                   NotificationRegistrar* registrar)
      : monitor_(monitor), registrar_(registrar) {}
  virtual ~RegistrationTask() {}

  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // It would be nice if we could specify a Source for each specific function
    // we wanted to observe, but the actual function objects are allocated on
    // the fly so there is no reliable object to point to (same problem if we
    // wanted to use the string name).  Thus, we use all sources and filter in
    // Observe.
    registrar_->Add(monitor_, NotificationType::EXTENSION_BOOKMARKS_API_INVOKED,
                    NotificationService::AllSources());
  }

 private:
  ExtensionsActivityMonitor* monitor_;
  NotificationRegistrar* registrar_;
  DISALLOW_COPY_AND_ASSIGN(RegistrationTask);
};
}  // namespace

ExtensionsActivityMonitor::ExtensionsActivityMonitor() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          new RegistrationTask(this, &registrar_));
}

ExtensionsActivityMonitor::~ExtensionsActivityMonitor() {
  // In some unrelated unit tests, there is no running UI loop.  In this case,
  // the PostTask in our ctor will not result in anything running, so |this|
  // won't be used for anything. In this case (or whenever no registration took
  // place) and only this case we allow destruction on another loop, but this
  // isn't something a client of this class can control; it happens implicitly
  // by not having a running UI thread.
  if (!registrar_.IsEmpty()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // The registrar calls RemoveAll in its dtor (which would happen in a
    // moment but explicitly call this so it is clear why we need to be on the
    // ui_loop_.
    registrar_.RemoveAll();
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

void ExtensionsActivityMonitor::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  base::AutoLock lock(records_lock_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const Extension* extension = Source<const Extension>(source).ptr();
  const BookmarksFunction* f = Details<const BookmarksFunction>(details).ptr();
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
