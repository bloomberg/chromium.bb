// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/balloon_collection_impl_ash.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/ash/balloon_view_ash.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#include "chrome/browser/ui/views/notifications/balloon_view_views.h"

namespace {

bool IsAshNotifyEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshNotifyDisabled);
}

}  // namespace

BalloonCollectionImplAsh::BalloonCollectionImplAsh() {
  if (IsAshNotifyEnabled()) {
    ash::Shell::GetInstance()->status_area_widget()->
        web_notification_tray()->SetDelegate(this);
  }
}

BalloonCollectionImplAsh::~BalloonCollectionImplAsh() {
}

bool BalloonCollectionImplAsh::HasSpace() const {
  if (!IsAshNotifyEnabled())
    return BalloonCollectionImpl::HasSpace();
  return true;  // Overflow is handled by ash::WebNotificationTray.
}

void BalloonCollectionImplAsh::Add(const Notification& notification,
                                   Profile* profile) {
  if (IsAshNotifyEnabled()) {
    if (notification.is_html())
      return;  // HTML notifications are not supported in Ash.
    if (notification.title().empty() && notification.body().empty())
      return;  // Empty notification, don't show.
  }
  return BalloonCollectionImpl::Add(notification, profile);
}

void BalloonCollectionImplAsh::DisableExtension(
    const std::string& notifcation_id) {
  Balloon* balloon = base().FindBalloonById(notifcation_id);
  if (!balloon)
    return;
  ExtensionService* extension_service =
      balloon->profile()->GetExtensionService();
  const GURL& origin = balloon->notification().origin_url();
  const extensions::Extension* extension =
      extension_service->extensions()->GetExtensionOrAppByURL(
          ExtensionURLInfo(origin));
  if (!extension)
    return;
  extension_service->DisableExtension(
      extension->id(), extensions::Extension::DISABLE_USER_ACTION);
}

void BalloonCollectionImplAsh::DisableNotificationsFromSource(
    const std::string& notifcation_id) {
  Balloon* balloon = base().FindBalloonById(notifcation_id);
  if (!balloon)
    return;
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(balloon->profile());
  service->DenyPermission(balloon->notification().origin_url());
}

void BalloonCollectionImplAsh::NotificationRemoved(
    const std::string& notifcation_id) {
  RemoveById(notifcation_id);
}

void BalloonCollectionImplAsh::ShowSettings(const std::string& notifcation_id) {
  Balloon* balloon = base().FindBalloonById(notifcation_id);
  Profile* profile =
      balloon ? balloon->profile() : ProfileManager::GetDefaultProfile();
  Browser* browser = browser::FindOrCreateTabbedBrowser(profile);
  browser->ShowContentSettingsPage(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void BalloonCollectionImplAsh::OnClicked(const std::string& notifcation_id) {
  Balloon* balloon = base().FindBalloonById(notifcation_id);
  if (!balloon)
    return;
  balloon->OnClick();
}

bool BalloonCollectionImplAsh::AddWebUIMessageCallback(
    const Notification& notification,
    const std::string& message,
    const chromeos::BalloonViewHost::MessageCallback& callback) {
#if defined(OS_CHROMEOS)
  Balloon* balloon = base().FindBalloon(notification);
  if (!balloon)
    return false;

  BalloonHost* balloon_host = balloon->balloon_view()->GetHost();
  if (!balloon_host)
    return false;
  chromeos::BalloonViewHost* balloon_view_host =
      static_cast<chromeos::BalloonViewHost*>(balloon_host);
  return balloon_view_host->AddWebUIMessageCallback(message, callback);
#else
  return false;
#endif
}

void BalloonCollectionImplAsh::AddSystemNotification(
    const Notification& notification,
    Profile* profile,
    bool sticky) {
  system_notifications_.insert(notification.notification_id());

  // Add balloons to the front of the stack. This ensures that system
  // notifications will always be displayed. NOTE: This has the side effect
  // that system notifications are displayed in inverse order, with the most
  // recent notification always at the front of the list.
  AddImpl(notification, profile, true /* add to front*/);
}

bool BalloonCollectionImplAsh::UpdateNotification(
    const Notification& notification) {
  Balloon* balloon = base().FindBalloon(notification);
  if (!balloon)
    return false;
  balloon->Update(notification);
  return true;
}

bool BalloonCollectionImplAsh::UpdateAndShowNotification(
    const Notification& notification) {
  return UpdateNotification(notification);
}

Balloon* BalloonCollectionImplAsh::MakeBalloon(
    const Notification& notification, Profile* profile) {
  Balloon* balloon = new Balloon(notification, profile, this);
  if (!IsAshNotifyEnabled()) {
    ::BalloonViewImpl* balloon_view = new ::BalloonViewImpl(this);
    if (system_notifications_.find(notification.notification_id()) !=
        system_notifications_.end())
      balloon_view->set_enable_web_ui(true);
    balloon->set_view(balloon_view);
    gfx::Size size(layout().min_balloon_width(), layout().min_balloon_height());
    balloon->set_content_size(size);
  } else {
    BalloonViewAsh* balloon_view = new BalloonViewAsh(this);
    balloon->set_view(balloon_view);
  }
  return balloon;
}

// For now, only use BalloonCollectionImplAsh on ChromeOS, until
// system_notifications_ is replaced with status area notifications.
#if defined(OS_CHROMEOS)
// static
BalloonCollection* BalloonCollection::Create() {
  return new BalloonCollectionImplAsh();
}
#endif
