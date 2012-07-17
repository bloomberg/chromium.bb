// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

std::string TimeToString(base::Time time) {
  int64 time_int64 = time.ToInternalValue();
  return base::Int64ToString(time_int64);
}

bool StringToTime(std::string time, base::Time* output) {
  int64 time_int64 = 0;
  if (!base::StringToInt64(time, &time_int64))
    return false;
  *output = base::Time::FromInternalValue(time_int64);
  return true;
}

}  // namespace

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
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  database_ = Database::Create(database_path_);
}

void PerformanceMonitor::FinishInit() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RegisterForNotifications();
  CheckForUncleanExits();
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread,
                 base::Unretained(this)));

  timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(2),
               this, &PerformanceMonitor::DoTimedCollections);

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
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::NotificationService::AllSources());

  // Crashes
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_HANG,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());

  // Profiles (for unclean exit)
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
      content::NotificationService::AllSources());
}

// We check if profiles exited cleanly initialization time in case they were
// loaded prior to PerformanceMonitor's initialization. Later profiles will be
// checked through the PROFILE_ADDED notification.
void PerformanceMonitor::CheckForUncleanExits() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  for (std::vector<Profile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if (!(*iter)->DidLastSessionExitCleanly()) {
      BrowserThread::PostBlockingPoolSequencedTask(
          Database::kDatabaseSequenceToken,
          FROM_HERE,
          base::Bind(&PerformanceMonitor::AddUncleanExitEvent,
                     base::Unretained(this),
                     (*iter)->GetDebugName()));
    }
  }
}

void PerformanceMonitor::AddUncleanExitEvent(const std::string& profile_name) {
  std::string database_key = kStateProfilePrefix + profile_name;
  std::string last_active_string = database_->GetStateValue(database_key);

  // Check if there was no previous time; this should only happen if the profile
  // was last used prior to PerformanceMonitor's integration. Do nothing in this
  // case, since the event was prior to the beginning of our recording.
  if (last_active_string.empty())
    return;

  base::Time last_active_time;
  CHECK(StringToTime(last_active_string, &last_active_time));

  scoped_ptr<Event> event =
      util::CreateUncleanExitEvent(last_active_time, profile_name);

  database_->AddEvent(*event.get());
}

void PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread() {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void PerformanceMonitor::UpdateLiveProfiles() {
  std::string time = TimeToString(base::Time::Now());
  scoped_ptr<std::set<std::string> > active_profiles(
      new std::set<std::string>());

  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    active_profiles->insert((*iter)->profile()->GetDebugName());
  }

  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::UpdateLiveProfilesHelper,
                 base::Unretained(this),
                 base::Passed(active_profiles.Pass()),
                 time));
}

void PerformanceMonitor::UpdateLiveProfilesHelper(
    scoped_ptr<std::set<std::string> > active_profiles,
    std::string time) {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (std::set<std::string>::const_iterator iter = active_profiles->begin();
       iter != active_profiles->end(); ++iter) {
    database_->AddStateValue(kStateProfilePrefix + *iter, time);
  }
}

void PerformanceMonitor::DoTimedCollections() {
  UpdateLiveProfiles();
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
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      AddEvent(util::CreateExtensionUninstallEvent(base::Time::Now(),
                                                   extension->id(),
                                                   extension->name(),
                                                   extension->url().spec(),
                                                   extension->location(),
                                                   extension->VersionString(),
                                                   extension->description()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_HANG: {
      content::WebContents* contents =
          content::Source<content::WebContents>(source).ptr();
      AddEvent(util::CreateRendererFreezeEvent(base::Time::Now(),
                                               contents->GetURL().spec()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost::RendererClosedDetails closed_details =
          *content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr();

      // We only care if this is an invalid termination.
      if (closed_details.status == base::TERMINATION_STATUS_NORMAL_TERMINATION
          || closed_details.status == base::TERMINATION_STATUS_STILL_RUNNING)
        break;

      // Determine the type of crash.
      EventType type =
          closed_details.status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
          EVENT_KILLED_BY_OS_CRASH : EVENT_RENDERER_CRASH;

      AddEvent(util::CreateCrashEvent(base::Time::Now(),
                                      type));
      break;
    }
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->DidLastSessionExitCleanly()) {
        BrowserThread::PostBlockingPoolSequencedTask(
            Database::kDatabaseSequenceToken,
            FROM_HERE,
            base::Bind(&PerformanceMonitor::AddUncleanExitEvent,
                       base::Unretained(this),
                       profile->GetDebugName()));
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

}  // namespace performance_monitor
