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

using extensions::Extension;

namespace performance_monitor {

PerformanceMonitor::PerformanceMonitor() : database_(NULL) {
}

PerformanceMonitor::~PerformanceMonitor() {
}

void PerformanceMonitor::Start() {
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&PerformanceMonitor::InitOnBackgroundThread,
                 base::Unretained(this)),
      base::Bind(&PerformanceMonitor::FinishInit,
                 base::Unretained(this)));
}

void PerformanceMonitor::InitOnBackgroundThread() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  database_ = Database::Create(database_path_);
}

bool PerformanceMonitor::SetDatabasePath(const FilePath& path) {
  if (!database_.get()) {
    database_path_ = path;
    return true;
  }

  // PerformanceMonitor already initialized with another path.
  return false;
}

void PerformanceMonitor::FinishInit() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RegisterForNotifications();
  CheckForVersionUpdate();
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

// Static
PerformanceMonitor* PerformanceMonitor::GetInstance() {
  return Singleton<PerformanceMonitor>::get();
}

void PerformanceMonitor::AddEvent(scoped_ptr<Event> event) {
  content::BrowserThread::PostBlockingPoolSequencedTask(
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
    std::string key, base::Callback<void(std::string)> callback) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string state_value = database_->GetStateValue(key);

  callback.Run(state_value);
}

void PerformanceMonitor::CheckForVersionUpdate() {
  base::Callback<void(std::string)> callback =
      base::Bind(&PerformanceMonitor::CheckForVersionUpdateHelper,
                 base::Unretained(this));

  content::BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::GetStateValueOnBackgroundThread,
                 base::Unretained(this),
                 std::string(kStateChromeVersion),
                 callback));
}

void PerformanceMonitor::CheckForVersionUpdateHelper(
    std::string previous_version) {
  chrome::VersionInfo version;
  DCHECK(version.is_valid());

  std::string current_version = version.Version();

  // We should never have a current_version which is older than the
  // previous_version.
  DCHECK(current_version >= previous_version);

  // If this is the first run, there will not be a stored value for Chrome
  // version; we insert the current version and will insert an event for the
  // next update of Chrome. If the previous version is older than the current
  // version, update the state in the database and insert an event.
  if (current_version > previous_version) {
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&Database::AddStateValue),
                   base::Unretained(database_.get()),
                   std::string(kStateChromeVersion),
                   current_version));
    if (!previous_version.empty()) {
      AddEvent(util::CreateChromeUpdateEvent(
          base::Time::Now(),
          previous_version,
          current_version));
    }
  }
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
      break;
    }
  }
}

}  // namespace performance_monitor
