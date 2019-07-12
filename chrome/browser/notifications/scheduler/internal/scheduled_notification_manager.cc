// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include <algorithm>
#include <map>
#include <unordered_set>

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {
namespace {

// Comparator used to sort notification entries based on creation time.
bool CreateTimeCompare(const NotificationEntry* lhs,
                       const NotificationEntry* rhs) {
  DCHECK(lhs && rhs);
  return lhs->create_time <= rhs->create_time;
}

class ScheduledNotificationManagerImpl : public ScheduledNotificationManager {
 public:
  using NotificationStore = std::unique_ptr<CollectionStore<NotificationEntry>>;

  ScheduledNotificationManagerImpl(
      NotificationStore notification_store,
      std::unique_ptr<IconStore> icon_store,
      const std::vector<SchedulerClientType>& clients,
      const SchedulerConfig& config)
      : notification_store_(std::move(notification_store)),
        icon_store_(std::move(icon_store)),
        clients_(clients.begin(), clients.end()),
        delegate_(nullptr),
        config_(config) {}

 private:
  void Init(Delegate* delegate, InitCallback callback) override {
    DCHECK(!delegate_);
    delegate_ = delegate;

    notification_store_->InitAndLoad(base::BindOnce(
        &ScheduledNotificationManagerImpl::OnNotificationStoreInitialized,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // NotificationManager implementation.
  void ScheduleNotification(
      std::unique_ptr<NotificationParams> notification_params) override {
    DCHECK(notification_params);
    std::string guid = notification_params->guid;
    DCHECK(!guid.empty());
    auto type = notification_params->type;

    if (!clients_.count(type) ||
        (notifications_.count(type) && notifications_[type].count(guid))) {
      // TODO(xingliu): Report duplicate guid failure.
      return;
    }

    auto entry =
        std::make_unique<NotificationEntry>(notification_params->type, guid);
    entry->notification_data =
        std::move(notification_params->notification_data);
    entry->schedule_params = std::move(notification_params->schedule_params);
    auto* entry_ptr = entry.get();
    notifications_[type][guid] = std::move(entry);
    notification_store_->Add(
        guid, *entry_ptr,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationAdded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DisplayNotification(const std::string& guid) override {
    std::unique_ptr<NotificationEntry> entry;
    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      if (it->second.count(guid))
        entry = std::move(it->second[guid]);
    }

    DCHECK(entry);

    notifications_[entry->type].erase(entry->guid);
    if (notifications_[entry->type].empty())
      notifications_.erase(entry->type);

    notification_store_->Delete(
        guid,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationDeleted,
                       weak_ptr_factory_.GetWeakPtr()));

    if (delegate_)
      delegate_->DisplayNotification(std::move(entry));
  }

  void GetAllNotifications(Notifications* notifications) const override {
    DCHECK(notifications);
    notifications->clear();

    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      auto type = it->first;
      for (const auto& pair : it->second) {
        (*notifications)[type].emplace_back(pair.second.get());
      }
    }

    // Sort by creation time for each notification type.
    for (auto it = notifications->begin(); it != notifications->end(); ++it) {
      std::sort(it->second.begin(), it->second.end(), &CreateTimeCompare);
    }
  }

  void GetNotifications(
      SchedulerClientType type,
      std::vector<const NotificationEntry*>* notifications) const override {
    DCHECK(notifications);
    notifications->clear();
    const auto it = notifications_.find(type);
    if (it == notifications_.end())
      return;
    for (const auto& pair : it->second)
      notifications->emplace_back(pair.second.get());
  }

  void DeleteNotifications(SchedulerClientType type) override {
    if (!notifications_.count(type))
      return;
    auto it = notifications_[type].begin();
    while (it != notifications_[type].end()) {
      const auto& entry = *it->second;
      ++it;
      notification_store_->Delete(
          entry.guid,
          base::BindOnce(
              &ScheduledNotificationManagerImpl::OnNotificationDeleted,
              weak_ptr_factory_.GetWeakPtr()));
    }
    notifications_.erase(type);
  }

  // Sync with registered clients. Delete entrties in |notifications_| if
  // their clients are deprecated.
  void SyncRegisteredClients() {
    auto it = notifications_.begin();
    while (it != notifications_.end()) {
      auto type = it->first;
      it++;
      if (!clients_.count(type)) {
        DeleteNotifications(type);
      }
    }
  }

  void OnNotificationStoreInitialized(
      InitCallback callback,
      bool success,
      CollectionStore<NotificationEntry>::Entries entries) {
    if (!success) {
      std::move(callback).Run(false);
      return;
    }

    for (auto it = entries.begin(); it != entries.end(); it++) {
      auto* entry = it->get();
      // Prune expired notifications. Also delete them in db.
      bool expired = entry->create_time + config_.notification_expiration <=
                     base::Time::Now();
      if (expired) {
        notification_store_->Delete(
            entry->guid,
            base::BindOnce(
                &ScheduledNotificationManagerImpl::OnNotificationDeleted,
                weak_ptr_factory_.GetWeakPtr()));
      } else if (clients_.count(entry->type)) {
        notifications_[entry->type].emplace(entry->guid, std::move(*it));
      }
    }
    SyncRegisteredClients();

    icon_store_->Init(base::BindOnce(
        &ScheduledNotificationManagerImpl::OnIconStoreInitialized,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnIconStoreInitialized(InitCallback callback, bool success) {
    std::move(callback).Run(success);
  }

  void OnNotificationAdded(bool success) { NOTIMPLEMENTED(); }

  void OnNotificationDeleted(bool success) { NOTIMPLEMENTED(); }

  void OnIconAdded(bool success) { NOTIMPLEMENTED(); }

  void OnIconDeleted(bool success) { NOTIMPLEMENTED(); }

  NotificationStore notification_store_;
  std::unique_ptr<IconStore> icon_store_;
  const std::unordered_set<SchedulerClientType> clients_;
  Delegate* delegate_;
  std::map<SchedulerClientType,
           std::map<std::string, std::unique_ptr<NotificationEntry>>>
      notifications_;
  const SchedulerConfig& config_;
  base::WeakPtrFactory<ScheduledNotificationManagerImpl> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManagerImpl);
};
}  // namespace

// static
std::unique_ptr<ScheduledNotificationManager>
ScheduledNotificationManager::Create(
    std::unique_ptr<CollectionStore<NotificationEntry>> notification_store,
    std::unique_ptr<IconStore> icon_store,
    const std::vector<SchedulerClientType>& clients,
    const SchedulerConfig& config) {
  return std::make_unique<ScheduledNotificationManagerImpl>(
      std::move(notification_store), std::move(icon_store), clients, config);
}

ScheduledNotificationManager::ScheduledNotificationManager() = default;

ScheduledNotificationManager::~ScheduledNotificationManager() = default;

}  // namespace notifications
