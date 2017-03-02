// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_storage_monitor.h"

#include <map>
#include <utility>

#include "base/metrics/histogram_macros.h"
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
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/storage_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/views/constants.h"

using content::BrowserThread;

namespace extensions {

namespace {

// The rate at which we would like to observe storage events.
const int kStorageEventRateSec = 30;

// Set the thresholds for the first notification. Once a threshold is exceeded,
// it will be doubled to throttle notifications.
const int64_t kMBytes = 1024 * 1024;
const int64_t kExtensionInitialThreshold = 1000 * kMBytes;

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

bool ShouldGatherMetricsFor(const Extension* extension) {
  // We want to know the usage of hosted apps' storage.
  return ShouldMonitorStorageFor(extension) && extension->is_hosted_app();
}

const Extension* GetExtensionById(content::BrowserContext* context,
                                  const std::string& extension_id) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING);
}

void LogTemporaryStorageUsage(
    scoped_refptr<storage::QuotaManager> quota_manager,
    int64_t usage) {
  const storage::QuotaSettings& settings = quota_manager->settings();
  if (settings.per_host_quota > 0) {
    // Note we use COUNTS_100 (instead of PERCENT) because this can potentially
    // exceed 100%.
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.HostedAppUnlimitedStorageTemporaryStorageUsage",
        100.0 * usage / settings.per_host_quota);
  }
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
      int64_t next_threshold,
      const base::TimeDelta& rate,
      bool should_uma) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(quota_manager.get());

    GURL origin = site_url.GetOrigin();
    StorageState& state = origin_state_map_[origin];
    state.quota_manager = quota_manager;
    state.extension_id = extension_id;
    state.next_threshold = next_threshold;
    state.should_uma = should_uma;

    // We always observe persistent storage usage.
    storage::StorageObserver::MonitorParams params(
        storage::kStorageTypePersistent, origin, rate, false);
    quota_manager->AddStorageObserver(this, params);
    if (should_uma) {
      // And if this is for uma, we also observe temporary storage usage.
      MonitorParams temporary_params(
          storage::kStorageTypeTemporary, origin, rate, false);
      quota_manager->AddStorageObserver(this, temporary_params);
    }
  }

  // Updates the threshold for an extension already being monitored.
  void UpdateThresholdForExtension(const std::string& extension_id,
                                   int64_t next_threshold) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    for (OriginStorageStateMap::iterator it = origin_state_map_.begin();
         it != origin_state_map_.end(); ) {
      if (it->second.extension_id == extension_id) {
        storage::StorageObserver::Filter filter(
            storage::kStorageTypePersistent, it->first);
        it->second.quota_manager->RemoveStorageObserverForFilter(this, filter);
        // We also need to unregister temporary storage observation, if this was
        // being tracked for uma.
        if (it->second.should_uma) {
          storage::StorageObserver::Filter temporary_filter(
              storage::kStorageTypeTemporary, it->first);
          it->second.quota_manager->RemoveStorageObserverForFilter(this,
                                                                   filter);
        }
        origin_state_map_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  // Stop observing all storage events. Called during shutdown.
  void StopObserving() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

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

    // If |next_threshold| is -1, it signifies that we should not enforce (and
    // only track) storage for this extension.
    int64_t next_threshold;

    bool should_uma;

    StorageState() : next_threshold(-1), should_uma(false) {}
  };
  typedef std::map<GURL, StorageState> OriginStorageStateMap;

  ~StorageEventObserver() override {
    DCHECK(origin_state_map_.empty());
    StopObserving();
  }

  // storage::StorageObserver implementation.
  void OnStorageEvent(const Event& event) override {
    OriginStorageStateMap::iterator iter =
        origin_state_map_.find(event.filter.origin);
    if (iter == origin_state_map_.end())
      return;
    StorageState& state = iter->second;

    if (state.should_uma) {
      if (event.filter.storage_type == storage::kStorageTypePersistent) {
        UMA_HISTOGRAM_MEMORY_KB(
            "Extensions.HostedAppUnlimitedStoragePersistentStorageUsage",
            event.usage);
      } else {
        // We can't use the quota in the event because it assumes unlimited
        // storage.
        BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                                base::Bind(&LogTemporaryStorageUsage,
                                           state.quota_manager, event.usage));
      }
    }

    if (state.next_threshold != -1 &&
        event.usage >= state.next_threshold) {
      while (event.usage >= state.next_threshold)
        state.next_threshold *= 2;

      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ExtensionStorageMonitor::OnStorageThresholdExceeded,
                     storage_monitor_,
                     state.extension_id,
                     state.next_threshold,
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
      observer_rate_(base::TimeDelta::FromSeconds(kStorageEventRateSec)),
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
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  StopMonitoringAll();
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
    const std::string& old_name) {
  if (!ShouldMonitorStorageFor(extension))
    return;

  if (!enable_for_all_extensions_) {
    // If monitoring is not enabled for installed extensions, just stop
    // monitoring.
    SetNextStorageThreshold(extension->id(), 0);
    StopMonitoringStorage(extension->id());
    return;
  }

  int64_t next_threshold = GetNextStorageThresholdFromPrefs(extension->id());
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

void ExtensionStorageMonitor::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const base::string16& error) {
  // We may get a lagging OnExtensionUninstalledDialogClosed() call during
  // testing, but did_start_uninstall should be false in this case.
  DCHECK(!uninstall_extension_id_.empty() || !did_start_uninstall);
  uninstall_extension_id_.clear();
}

std::string ExtensionStorageMonitor::GetNotificationId(
    const std::string& extension_id) {
  std::vector<std::string> placeholders;
  placeholders.push_back(context_->GetPath().BaseName().MaybeAsASCII());
  placeholders.push_back(extension_id);

  return base::ReplaceStringPlaceholders(
      kNotificationIdFormat, placeholders, NULL);
}

void ExtensionStorageMonitor::OnStorageThresholdExceeded(
    const std::string& extension_id,
    int64_t next_threshold,
    int64_t current_usage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

void ExtensionStorageMonitor::OnImageLoaded(const std::string& extension_id,
                                            int64_t current_usage,
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

  std::unique_ptr<message_center::Notification> notification;
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_EXTENSION_STORAGE_MONITOR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_STORAGE_MONITOR_TEXT,
          base::UTF8ToUTF16(extension->name()),
          base::Int64ToString16(current_usage / kMBytes)),
      notification_image, base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kSystemNotifierId),
      notification_data,
      new message_center::HandleNotificationButtonClickDelegate(
          base::Bind(&ExtensionStorageMonitor::OnNotificationButtonClick,
                     weak_ptr_factory_.GetWeakPtr(), extension_id))));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

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
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension.get() || !ShouldGatherMetricsFor(extension.get()))
    StopMonitoringStorage(extension_id);

  SetStorageNotificationEnabled(extension_id, false);

  message_center::MessageCenter::Get()->RemoveNotification(
      GetNotificationId(extension_id), false);
}

void ExtensionStorageMonitor::StartMonitoringStorage(
    const Extension* extension) {
  if (!ShouldMonitorStorageFor(extension))
    return;

  bool should_enforce = (enable_for_all_extensions_) &&
                        IsStorageNotificationEnabled(extension->id());

  bool for_metrics = ShouldGatherMetricsFor(extension);

  if (!should_enforce && !for_metrics)
    return;  // Don't track this extension.

  // Lazily create the storage monitor proxy on the IO thread.
  if (!storage_observer_.get()) {
    storage_observer_ =
        new StorageEventObserver(weak_ptr_factory_.GetWeakPtr());
  }

  GURL site_url = util::GetSiteForExtensionId(extension->id(), context_);
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(context_, site_url);
  DCHECK(storage_partition);
  scoped_refptr<storage::QuotaManager> quota_manager(
      storage_partition->GetQuotaManager());

  GURL storage_origin(site_url.GetOrigin());
  if (extension->is_hosted_app())
    storage_origin = AppLaunchInfo::GetLaunchWebURL(extension).GetOrigin();

  // Don't give a threshold if we're not enforcing.
  int next_threshold =
      should_enforce ? GetNextStorageThreshold(extension->id()) : -1;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StorageEventObserver::StartObservingForExtension,
                 storage_observer_,
                 quota_manager,
                 extension->id(),
                 storage_origin,
                 next_threshold,
                 observer_rate_,
                 for_metrics));
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
  uninstall_dialog_->ConfirmUninstall(
      extension, extensions::UNINSTALL_REASON_STORAGE_THRESHOLD_EXCEEDED,
      UNINSTALL_SOURCE_STORAGE_THRESHOLD_EXCEEDED);
}

int64_t ExtensionStorageMonitor::GetNextStorageThreshold(
    const std::string& extension_id) const {
  int next_threshold = GetNextStorageThresholdFromPrefs(extension_id);
  if (next_threshold == 0) {
    // The next threshold is written to the prefs after the initial threshold is
    // exceeded.
    next_threshold = initial_extension_threshold_;
  }
  return next_threshold;
}

void ExtensionStorageMonitor::SetNextStorageThreshold(
    const std::string& extension_id,
    int64_t next_threshold) {
  extension_prefs_->UpdateExtensionPref(
      extension_id,
      kPrefNextStorageThreshold,
      next_threshold > 0
          ? new base::StringValue(base::Int64ToString(next_threshold))
          : NULL);
}

int64_t ExtensionStorageMonitor::GetNextStorageThresholdFromPrefs(
    const std::string& extension_id) const {
  std::string next_threshold_str;
  if (extension_prefs_->ReadPrefAsString(
          extension_id, kPrefNextStorageThreshold, &next_threshold_str)) {
    int64_t next_threshold;
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
      extension_id, kPrefDisableStorageNotifications,
      enable_notifications ? NULL : new base::Value(true));
}

}  // namespace extensions
