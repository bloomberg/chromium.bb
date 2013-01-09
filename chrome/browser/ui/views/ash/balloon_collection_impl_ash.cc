// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/balloon_collection_impl_ash.h"

#include "ash/shell.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/ash/balloon_view_ash.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#include "chrome/browser/ui/views/notifications/balloon_view_views.h"

BalloonCollectionImplAsh::BalloonCollectionImplAsh() {
  ash::Shell::GetInstance()->GetWebNotificationTray()->message_center()->
      SetDelegate(this);
}

BalloonCollectionImplAsh::~BalloonCollectionImplAsh() {
}

bool BalloonCollectionImplAsh::HasSpace() const {
  return true;  // Overflow is handled by ash::WebNotificationTray.
}

void BalloonCollectionImplAsh::Add(const Notification& notification,
                                   Profile* profile) {
  if (notification.is_html())
    return;  // HTML notifications are not supported in Ash.
  if (notification.title().empty() && notification.body().empty())
    return;  // Empty notification, don't show.
  return BalloonCollectionImpl::Add(notification, profile);
}

void BalloonCollectionImplAsh::DisableExtension(
    const std::string& notification_id) {
  Balloon* balloon = base().FindBalloonById(notification_id);
  const extensions::Extension* extension = GetBalloonExtension(balloon);
  if (!extension)
    return;
  balloon->profile()->GetExtensionService()->DisableExtension(
      extension->id(), extensions::Extension::DISABLE_USER_ACTION);
}

void BalloonCollectionImplAsh::DisableNotificationsFromSource(
    const std::string& notification_id) {
  Balloon* balloon = base().FindBalloonById(notification_id);
  if (!balloon)
    return;
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(balloon->profile());
  service->DenyPermission(balloon->notification().origin_url());
}

void BalloonCollectionImplAsh::NotificationRemoved(
    const std::string& notification_id) {
  RemoveById(notification_id);
}

void BalloonCollectionImplAsh::ShowSettings(
    const std::string& notification_id) {
  Balloon* balloon = base().FindBalloonById(notification_id);
  Profile* profile =
      balloon ? balloon->profile() : ProfileManager::GetDefaultProfile();
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(profile,
                                        chrome::HOST_DESKTOP_TYPE_ASH);
  if (GetBalloonExtension(balloon))
    chrome::ShowExtensions(browser);
  else
    chrome::ShowContentSettings(browser, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void BalloonCollectionImplAsh::OnClicked(const std::string& notification_id) {
  Balloon* balloon = base().FindBalloonById(notification_id);
  if (!balloon)
    return;
  balloon->OnClick();
}

void BalloonCollectionImplAsh::OnButtonClicked(
    const std::string& notification_id, int button_index) {
  Balloon* balloon = base().FindBalloonById(notification_id);
  if (balloon)
    balloon->OnButtonClick(button_index);
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
  BalloonViewAsh* balloon_view = new BalloonViewAsh(this);
  balloon->set_view(balloon_view);
  return balloon;
}

const extensions::Extension* BalloonCollectionImplAsh::GetBalloonExtension(
    Balloon* balloon) {
  if (!balloon)
    return NULL;
  ExtensionService* extension_service =
      balloon->profile()->GetExtensionService();
  const GURL& origin = balloon->notification().origin_url();
  return extension_service->extensions()->GetExtensionOrAppByURL(
      ExtensionURLInfo(origin));
}

#if defined(OS_CHROMEOS)
// static
BalloonCollection* BalloonCollection::Create() {
  return new BalloonCollectionImplAsh();
}
#endif
