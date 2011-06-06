// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

////////////////////////// NotificationUIManager //////////////////////////////

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  return new NotificationUIManager(local_state);
}

NotificationUIManager::NotificationUIManager(PrefService* local_state) {
}

NotificationUIManager::~NotificationUIManager() {
}

// static
void NotificationUIManager::RegisterPrefs(PrefService* prefs) {
}

void NotificationUIManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
}

void NotificationUIManager::Add(const Notification& notification,
                                Profile* profile) {
}

bool NotificationUIManager::CancelById(const std::string& notification_id) {
  return true;
}

bool NotificationUIManager::CancelAllBySourceOrigin(const GURL& source_origin) {
  return true;
}

void NotificationUIManager::OnBalloonSpaceChanged() {
}

/////////////////////////// DesktopNotificationService ////////////////////////

DesktopNotificationService::DesktopNotificationService(Profile* profile,
    NotificationUIManager* ui_manager) {
}

DesktopNotificationService::~DesktopNotificationService() {
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin, int process_id, int route_id, int callback_context,
    TabContents* tab) {
}

void DesktopNotificationService::RegisterUserPrefs(PrefService* user_prefs) {
  content_settings::NotificationProvider::RegisterUserPrefs(user_prefs);
}

void DesktopNotificationService::GrantPermission(const GURL& origin) {
}

void DesktopNotificationService::DenyPermission(const GURL& origin) {
}

void DesktopNotificationService::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
}

ContentSetting DesktopNotificationService::GetDefaultContentSetting() {
  return CONTENT_SETTING_DEFAULT;
}

void DesktopNotificationService::SetDefaultContentSetting(
    ContentSetting setting) {
}

bool DesktopNotificationService::IsDefaultContentSettingManaged() const {
  return false;
}

std::vector<GURL> DesktopNotificationService::GetAllowedOrigins() {
  return std::vector<GURL>();
}

std::vector<GURL> DesktopNotificationService::GetBlockedOrigins() {
  return std::vector<GURL>();
}

void DesktopNotificationService::ResetAllowedOrigin(const GURL& origin) {
}

void DesktopNotificationService::ResetBlockedOrigin(const GURL& origin) {
}

int DesktopNotificationService::HasPermission(const GURL& origin) {
  return WebKit::WebNotificationPresenter::PermissionNotAllowed;
}

// static
DesktopNotificationService* DesktopNotificationServiceFactory::GetForProfile(
    Profile* profile) {
  return NULL;
}

// static
string16 DesktopNotificationService::CreateDataUrl(
    const GURL& icon_url,
    const string16& title,
    const string16& body,
    WebKit::WebTextDirection dir) {
  return string16();
}

///////////////////// DesktopNotificationServiceFactory ///////////////////////

// static
DesktopNotificationServiceFactory* DesktopNotificationServiceFactory::
    GetInstance() {
  return Singleton<DesktopNotificationServiceFactory>::get();
}

DesktopNotificationServiceFactory::DesktopNotificationServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

DesktopNotificationServiceFactory::~DesktopNotificationServiceFactory() {
}

ProfileKeyedService* DesktopNotificationServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return NULL;
}

bool DesktopNotificationServiceFactory::ServiceHasOwnInstanceInIncognito() {
  return true;
}

/////////////////////////////////// BalloonHost ///////////////////////////////

// static
bool BalloonHost::IsRenderViewReady() const {
  return true;
}

//////////////////////////// NotificationsPrefsCache //////////////////////////

// static
void NotificationsPrefsCache::ListValueToGurlVector(
    const ListValue& origin_list,
    std::vector<GURL>* origin_vector) {
}


Notification::Notification(const GURL& origin_url,
                           const GURL& content_url,
                           const string16& display_source,
                           const string16& replace_id,
                           NotificationDelegate* delegate) {
}

Notification::~Notification() {
}
