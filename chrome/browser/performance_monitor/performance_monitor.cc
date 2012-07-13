// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;
using extensions::Extension;

namespace performance_monitor {

PerformanceMonitor::PerformanceMonitor() : database_(NULL) {
}

PerformanceMonitor::~PerformanceMonitor() {
}

bool PerformanceMonitor::SetDatabasePath(const FilePath& path) {
  if (!database_.get()) {
    database_path_ = path;
    return true;
  }

  // PerformanceMonitor already initialized with another path.
  return false;
}

// static
PerformanceMonitor* PerformanceMonitor::GetInstance() {
  return Singleton<PerformanceMonitor>::get();
}

void PerformanceMonitor::Start() {
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&PerformanceMonitor::InitOnBackgroundThread,
                 base::Unretained(this)),
      base::Bind(&PerformanceMonitor::FinishInit,
                 base::Unretained(this)));
}

void PerformanceMonitor::InitOnBackgroundThread() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  database_ = Database::Create(database_path_);
}

void PerformanceMonitor::FinishInit() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RegisterForNotifications();
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread,
                 base::Unretained(this)));

  // Post a task to the background thread to a function which does nothing.
  // This will force any tasks the database is performing to finish prior to
  // the reply being sent, since they use the same thread.
  //
  // Important! Make sure that methods in FinishInit() only rely on posting
  // to the background thread, and do not rely upon a reply from the background
  // thread; this is necessary for this notification to be valid.
  util::PostTaskToDatabaseThreadAndReply(
      base::Bind(&base::DoNothing),
      base::Bind(&PerformanceMonitor::NotifyInitialized,
                 base::Unretained(this)));
}

void PerformanceMonitor::RegisterForNotifications() {
  // Extensions
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_ENABLED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
}

void PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  chrome::VersionInfo version;
  DCHECK(version.is_valid());
  std::string current_version = version.Version();

  std::string previous_version = database_->GetStateValue(kStateChromeVersion);

  // We should never have a current_version which is older than the
  // previous_version.
  DCHECK(current_version >= previous_version);

  // If this is the first run, there will not be a stored value for Chrome
  // version; we insert the current version and will insert an event for the
  // next update of Chrome. If the previous version is older than the current
  // version, update the state in the database and insert an event.
  if (current_version > previous_version) {
    database_->AddStateValue(kStateChromeVersion, current_version);
    if (!previous_version.empty()) {
      scoped_ptr<Event> event = util::CreateChromeUpdateEvent(
          base::Time::Now(), previous_version, current_version);
      database_->AddEvent(*event.get());
    }
  }
}

void PerformanceMonitor::AddEvent(scoped_ptr<Event> event) {
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::AddEventOnBackgroundThread,
                 base::Unretained(this),
                 base::Passed(event.Pass())));
}

void PerformanceMonitor::AddEventOnBackgroundThread(scoped_ptr<Event> event) {
  database_->AddEvent(*event.get());
}

void PerformanceMonitor::GetStateValueOnBackgroundThread(
    const std::string& key,
    const StateValueCallback& callback) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string state_value = database_->GetStateValue(key);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(callback, state_value));
}

void PerformanceMonitor::NotifyInitialized() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED,
      content::Source<PerformanceMonitor>(this),
      content::NotificationService::NoDetails());
}

void PerformanceMonitor::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      AddEvent(util::CreateExtensionInstallEvent(base::Time::Now(),
                                                 extension->id(),
                                                 extension->name(),
                                                 extension->url().spec(),
                                                 extension->location(),
                                                 extension->VersionString(),
                                                 extension->description()));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_ENABLED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      AddEvent(util::CreateExtensionEnableEvent(base::Time::Now(),
                                                extension->id(),
                                                extension->name(),
                                                extension->url().spec(),
                                                extension->location(),
                                                extension->VersionString(),
                                                extension->description()));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::UnloadedExtensionInfo* info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      const Extension* extension = info->extension;
      AddEvent(util::CreateExtensionUnloadEvent(base::Time::Now(),
                                                extension->id(),
                                                extension->name(),
                                                extension->url().spec(),
                                                extension->location(),
                                                extension->VersionString(),
                                                extension->description(),
                                                info->reason));
      break;
    }
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      const CrxInstaller* installer =
        content::Source<CrxInstaller>(source).ptr();

      // Check if the reason for the install was due to an extension update.
      if (installer->install_cause() != extension_misc::INSTALL_CAUSE_UPDATE)
        break;

      const Extension* extension = content::Details<Extension>(details).ptr();
      AddEvent(util::CreateExtensionUpdateEvent(base::Time::Now(),
                                                extension->id(),
                                                extension->name(),
                                                extension->url().spec(),
                                                extension->location(),
                                                extension->VersionString(),
                                                extension->description()));
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

}  // namespace performance_monitor
