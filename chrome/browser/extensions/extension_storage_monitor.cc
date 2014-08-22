// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_storage_monitor.h"

#include <map>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_storage_monitor_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/views/constants.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/storage_observer.h"

using content::BrowserThread;

namespace extensions {

namespace {

// The rate at which we would like to observe storage events.
const int kStorageEventRateSec = 30;

// The storage type to monitor.
const storage::StorageType kMonitorStorageType =
    storage::kStorageTypePersistent;

// Set the thresholds for the first notification. Ephemeral apps have a lower
// threshold than installed extensions and apps. Once a threshold is exceeded,
// it will be doubled to throttle notifications.
const int64 kMBytes = 1024 * 1024;
const int64 kEphemeralAppInitialThreshold = 250 * kMBytes;
const int64 kExtensionInitialThreshold = 1000 * kMBytes;

// Notifications have an ID so that we can update them.
const char kNotificationIdFormat[] = "ExtensionStorageMonitor-$1-$2";
const char kSystemNotifierId[] = "ExtensionStorageMonitor";

// A preference that stores the next threshold for displaying a notification
// when an extension or app consumes excessive disk space. This will not be
// set until the extension/app reaches the initial threshold.
const char kPrefNextStorageThreshold[] = "next_storage_threshold";

// If this preference is set to true, notifications will be suppressed when an
// extension or app consumes excessive disk space.
const char kPrefDisableStorageNotifications[] = "disable_storage_notifications";

bool ShouldMonitorStorageFor(const Extension* extension) {
  // Only monitor storage for extensions that are granted unlimited storage.
  // Do not monitor storage for component extensions.
  return extension->permissions_data()->HasAPIPermission(
             APIPermission::kUnlimitedStorage) &&
         extension->location() != Manifest::COMPONENT;
}

const Extension* GetExtensionById(content::BrowserContext* context,
                                  const std::string& extension_id) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING);
}

}  // namespace

// StorageEventObserver monitors the storage usage of extensions and lives on
// the IO thread. When a threshold is exceeded, a message will be posted to the
// UI thread, which displays the notification.
class StorageEventObserver
    : public base::RefCountedThreadSafe<StorageEventObserver,
                                        BrowserThread::DeleteOnIOThread>,
      public storage::StorageObserver {
 public:
  explicit StorageEventObserver(
      base::WeakPtr<ExtensionStorageMonitor> storage_monitor)
      : storage_monitor_(storage_monitor) {
  }

  // Register as an observer for the extension's storage events.
  void StartObservingForExtension(
      scoped_refptr<storage::QuotaManager> quota_manager,
      const std::string& extension_id,
      const GURL& site_url,
      int64 next_threshold,
      int rate) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(quota_manager.get());

    GURL origin = site_url.GetOrigin();
    StorageState& state = origin_state_map_[origin];
    state.quota_manager = quota_manager;
    state.extension_id = extension_id;
    state.next_threshold = next_threshold;

    storage::StorageObserver::MonitorParams params(
        kMonitorStorageType, origin, base::TimeDelta::FromSeconds(rate), false);
    quota_manager->AddStorageObserver(this, params);
  }

  // Updates the threshold for an extension already being monitored.
  void UpdateThresholdForExtension(const std::string& extension_id,
                                   int64 next_threshold) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    for (OriginStorageStateMap::iterator it = origin_state_map_.begin();
         it != origin_state_map_.end();
         ++it) {
      if (it->second.extension_id == extension_id) {
        it->second.next_threshold = next_threshold;
        break;
      }
    }
  }

  // Deregister as an observer for the extension's storage events.
  void StopObservingForExtension(const std::string& extension_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    for (OriginStorageStateMap::iterator it = origin_state_map_.begin();
         it != origin_state_map_.end(); ) {
      if (it->second.extension_id == extension_id) {
        storage::StorageObserver::Filter filter(kMonitorStorageType, it->first);
        it->second.quota_manager->RemoveStorageObserverForFilter(this, filter);
        origin_state_map_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  // Stop observing all storage events. Called during shutdown.
  void StopObserving() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    for (OriginStorageStateMap::iterator it = origin_state_map_.begin();
         it != origin_state_map_.end(); ++it) {
      it->second.quota_manager->RemoveStorageObserver(this);
    }
    origin_state_map_.clear();
  }

 private:
  friend class base::DeleteHelper<StorageEventObserver>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;

  struct StorageState {
    scoped_refptr<storage::QuotaManager> quota_manager;
    std::string extension_id;
    int64 next_threshold;

    StorageState() : next_threshold(0) {}
  };
  typedef std::map<GURL, StorageState> OriginStorageStateMap;

  virtual ~StorageEventObserver() {
    DCHECK(origin_state_map_.empty());
    StopObserving();
  }

  // storage::StorageObserver implementation.
  virtual void OnStorageEvent(const Event& event) OVERRIDE {
    OriginStorageStateMap::iterator state =
        origin_state_map_.find(event.filter.origin);
    if (state == origin_state_map_.end())
      return;

    if (event.usage >= state->second.next_threshold) {
      while (event.usage >= state->second.next_threshold)
        state->second.next_threshold *= 2;

      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ExtensionStorageMonitor::OnStorageThresholdExceeded,
                     storage_monitor_,
                     state->second.extension_id,
                     state->second.next_threshold,
                     event.usage));
    }
  }

  OriginStorageStateMap origin_state_map_;
  base::WeakPtr<ExtensionStorageMonitor> storage_monitor_;
};

// ExtensionStorageMonitor

// static
ExtensionStorageMonitor* ExtensionStorageMonitor::Get(
    content::BrowserContext* context) {
  return ExtensionStorageMonitorFactory::GetForBrowserContext(context);
}

ExtensionStorageMonitor::ExtensionStorageMonitor(
    content::BrowserContext* context)
    : enable_for_all_extensions_(false),
      initial_extension_threshold_(kExtensionInitialThreshold),
      initial_ephemeral_threshold_(kEphemeralAppInitialThreshold),
      observer_rate_(kStorageEventRateSec),
      context_(context),
      extension_prefs_(ExtensionPrefs::Get(context)),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(extension_prefs_);

  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<content::BrowserContext>(context_));

  extension_registry_observer_.Add(ExtensionRegistry::Get(context_));
}

ExtensionStorageMonitor::~ExtensionStorageMonitor() {}

void ExtensionStorageMonitor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      StopMonitoringAll();
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionStorageMonitor::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  StartMonitoringStorage(extension);
}

void ExtensionStorageMonitor::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  StopMonitoringStorage(extension->id());
}

void ExtensionStorageMonitor::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  // If an ephemeral app was promoted to a regular installed app, we may need to
  // increase its next threshold.
  if (!from_ephemeral || !ShouldMonitorStorageFor(extension))
    return;

  if (!enable_for_all_extensions_) {
    // If monitoring is not enabled for installed extensions, just stop
    // monitoring.
    SetNextStorageThreshold(extension->id(), 0);
    StopMonitoringStorage(extension->id());
    return;
  }

  int64 next_threshold = GetNextStorageThresholdFromPrefs(extension->id());
  if (next_threshold <= initial_extension_threshold_) {
    // Clear the next threshold in the prefs. This effectively raises it to
    // |initial_extension_threshold_|. If the current threshold is already
    // higher than this, leave it as is.
    SetNextStorageThreshold(extension->id(), 0);

    if (storage_observer_.get()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&StorageEventObserver::UpdateThresholdForExtension,
                     storage_observer_,
                     extension->id(),
                     initial_extension_threshold_));
    }
  }
}

void ExtensionStorageMonitor::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveNotificationForExtension(extension->id());
}

void ExtensionStorageMonitor::ExtensionUninstallAccepted() {
  DCHECK(!uninstall_extension_id_.empty());

  const Extension* extension = GetExtensionById(context_,
                                                uninstall_extension_id_);
  uninstall_extension_id_.clear();
  if (!extension)
    return;

  ExtensionService* service =
      ExtensionSystem::Get(context_)->extension_service();
  DCHECK(service);
  service->UninstallExtension(
      extension->id(),
      extensions::UNINSTALL_REASON_STORAGE_THRESHOLD_EXCEEDED,
      base::Bind(&base::DoNothing),
      NULL);
}

void ExtensionStorageMonitor::ExtensionUninstallCanceled() {
  uninstall_extension_id_.clear();
}

std::string ExtensionStorageMonitor::GetNotificationId(
    const std::string& extension_id) {
  std::vector<std::string> placeholders;
  placeholders.push_back(context_->GetPath().BaseName().MaybeAsASCII());
  placeholders.push_back(extension_id);

  return ReplaceStringPlaceholders(kNotificationIdFormat, placeholders, NULL);
}

void ExtensionStorageMonitor::OnStorageThresholdExceeded(
    const std::string& extension_id,
    int64 next_threshold,
    int64 current_usage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension = GetExtensionById(context_, extension_id);
  if (!extension)
    return;

  if (GetNextStorageThreshold(extension->id()) < next_threshold)
    SetNextStorageThreshold(extension->id(), next_threshold);

  const int kIconSize = message_center::kNotificationIconSize;
  ExtensionResource resource =  IconsInfo::GetIconResource(
      extension, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  ImageLoader::Get(context_)->LoadImageAsync(
      extension, resource, gfx::Size(kIconSize, kIconSize),
      base::Bind(&ExtensionStorageMonitor::OnImageLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 current_usage));
}

void ExtensionStorageMonitor::OnImageLoaded(
    const std::string& extension_id,
    int64 current_usage,
    const gfx::Image& image) {
  const Extension* extension = GetExtensionById(context_, extension_id);
  if (!extension)
    return;

  // Remove any existing notifications to force a new notification to pop up.
  std::string notification_id(GetNotificationId(extension_id));
  message_center::MessageCenter::Get()->RemoveNotification(
      notification_id, false);

  message_center::RichNotificationData notification_data;
  notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(extension->is_app() ?
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_DISMISS_APP :
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_DISMISS_EXTENSION)));
  notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(extension->is_app() ?
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_UNINSTALL_APP :
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_UNINSTALL_EXTENSION)));

  gfx::Image notification_image(image);
  if (notification_image.IsEmpty()) {
    notification_image =
        extension->is_app() ? gfx::Image(util::GetDefaultAppIcon())
                            : gfx::Image(util::GetDefaultExtensionIcon());
  }

  scoped_ptr<message_center::Notification> notification;
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      notification_id,
      l10n_util::GetStringUTF16(IDS_EXTENSION_STORAGE_MONITOR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_STORAGE_MONITOR_TEXT,
          base::UTF8ToUTF16(extension->name()),
          base::IntToString16(current_usage / kMBytes)),
      notification_image,
      base::string16() /* display source */,
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT, kSystemNotifierId),
      notification_data,
      new message_center::HandleNotificationButtonClickDelegate(base::Bind(
          &ExtensionStorageMonitor::OnNotificationButtonClick,
          weak_ptr_factory_.GetWeakPtr(),
          extension_id))));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());

  notified_extension_ids_.insert(extension_id);
}

void ExtensionStorageMonitor::OnNotificationButtonClick(
    const std::string& extension_id, int button_index) {
  switch (button_index) {
    case BUTTON_DISABLE_NOTIFICATION: {
      DisableStorageMonitoring(extension_id);
      break;
    }
    case BUTTON_UNINSTALL: {
      ShowUninstallPrompt(extension_id);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionStorageMonitor::DisableStorageMonitoring(
    const std::string& extension_id) {
  StopMonitoringStorage(extension_id);

  SetStorageNotificationEnabled(extension_id, false);

  message_center::MessageCenter::Get()->RemoveNotification(
      GetNotificationId(extension_id), false);
}

void ExtensionStorageMonitor::StartMonitoringStorage(
    const Extension* extension) {
  if (!ShouldMonitorStorageFor(extension))
    return;

  // First apply this feature only to experimental ephemeral apps. If it works
  // well, roll it out to all extensions and apps.
  if (!enable_for_all_extensions_ &&
      !extension_prefs_->IsEphemeralApp(extension->id())) {
    return;
  }

  if (!IsStorageNotificationEnabled(extension->id()))
    return;

  // Lazily create the storage monitor proxy on the IO thread.
  if (!storage_observer_.get()) {
    storage_observer_ =
        new StorageEventObserver(weak_ptr_factory_.GetWeakPtr());
  }

  GURL site_url =
      extensions::util::GetSiteForExtensionId(extension->id(), context_);
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(context_, site_url);
  DCHECK(storage_partition);
  scoped_refptr<storage::QuotaManager> quota_manager(
      storage_partition->GetQuotaManager());

  GURL storage_origin(site_url.GetOrigin());
  if (extension->is_hosted_app())
    storage_origin = AppLaunchInfo::GetLaunchWebURL(extension).GetOrigin();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StorageEventObserver::StartObservingForExtension,
                 storage_observer_,
                 quota_manager,
                 extension->id(),
                 storage_origin,
                 GetNextStorageThreshold(extension->id()),
                 observer_rate_));
}

void ExtensionStorageMonitor::StopMonitoringStorage(
    const std::string& extension_id) {
  if (!storage_observer_.get())
    return;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StorageEventObserver::StopObservingForExtension,
                 storage_observer_,
                 extension_id));
}

void ExtensionStorageMonitor::StopMonitoringAll() {
  extension_registry_observer_.RemoveAll();

  RemoveAllNotifications();

  if (!storage_observer_.get())
    return;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StorageEventObserver::StopObserving, storage_observer_));
  storage_observer_ = NULL;
}

void ExtensionStorageMonitor::RemoveNotificationForExtension(
    const std::string& extension_id) {
  std::set<std::string>::iterator ext_id =
      notified_extension_ids_.find(extension_id);
  if (ext_id == notified_extension_ids_.end())
    return;

  notified_extension_ids_.erase(ext_id);
  message_center::MessageCenter::Get()->RemoveNotification(
      GetNotificationId(extension_id), false);
}

void ExtensionStorageMonitor::RemoveAllNotifications() {
  if (notified_extension_ids_.empty())
    return;

  message_center::MessageCenter* center = message_center::MessageCenter::Get();
  DCHECK(center);
  for (std::set<std::string>::iterator it = notified_extension_ids_.begin();
       it != notified_extension_ids_.end(); ++it) {
    center->RemoveNotification(GetNotificationId(*it), false);
  }
  notified_extension_ids_.clear();
}

void ExtensionStorageMonitor::ShowUninstallPrompt(
    const std::string& extension_id) {
  const Extension* extension = GetExtensionById(context_, extension_id);
  if (!extension)
    return;

  if (!uninstall_dialog_.get()) {
    uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
        Profile::FromBrowserContext(context_), NULL, this));
  }

  uninstall_extension_id_ = extension->id();
  uninstall_dialog_->ConfirmUninstall(extension);
}

int64 ExtensionStorageMonitor::GetNextStorageThreshold(
    const std::string& extension_id) const {
  int next_threshold = GetNextStorageThresholdFromPrefs(extension_id);
  if (next_threshold == 0) {
    // The next threshold is written to the prefs after the initial threshold is
    // exceeded.
    next_threshold = extension_prefs_->IsEphemeralApp(extension_id)
                         ? initial_ephemeral_threshold_
                         : initial_extension_threshold_;
  }
  return next_threshold;
}

void ExtensionStorageMonitor::SetNextStorageThreshold(
    const std::string& extension_id,
    int64 next_threshold) {
  extension_prefs_->UpdateExtensionPref(
      extension_id,
      kPrefNextStorageThreshold,
      next_threshold > 0
          ? new base::StringValue(base::Int64ToString(next_threshold))
          : NULL);
}

int64 ExtensionStorageMonitor::GetNextStorageThresholdFromPrefs(
    const std::string& extension_id) const {
  std::string next_threshold_str;
  if (extension_prefs_->ReadPrefAsString(
          extension_id, kPrefNextStorageThreshold, &next_threshold_str)) {
    int64 next_threshold;
    if (base::StringToInt64(next_threshold_str, &next_threshold))
      return next_threshold;
  }

  // A return value of zero indicates that the initial threshold has not yet
  // been reached.
  return 0;
}

bool ExtensionStorageMonitor::IsStorageNotificationEnabled(
    const std::string& extension_id) const {
  bool disable_notifications;
  if (extension_prefs_->ReadPrefAsBoolean(extension_id,
                                          kPrefDisableStorageNotifications,
                                          &disable_notifications)) {
    return !disable_notifications;
  }

  return true;
}

void ExtensionStorageMonitor::SetStorageNotificationEnabled(
    const std::string& extension_id,
    bool enable_notifications) {
  extension_prefs_->UpdateExtensionPref(
      extension_id,
      kPrefDisableStorageNotifications,
      enable_notifications ? NULL : new base::FundamentalValue(true));
}

}  // namespace extensions
