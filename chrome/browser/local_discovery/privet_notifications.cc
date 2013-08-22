// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_notifications.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace local_discovery {

namespace {
const int kTenMinutesInSeconds = 600;
const char kPrivetInfoKeyUptime[] = "uptime";
const char kPrivetNotificationIDPrefix[] = "privet_notification:";
const char kPrivetNotificationOriginUrl[] = "chrome://devices";
}

PrivetNotificationsListener::PrivetNotificationsListener(
    scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory,
    Delegate* delegate) : delegate_(delegate) {
  privet_http_factory_.swap(privet_http_factory);
}

PrivetNotificationsListener::~PrivetNotificationsListener() {
}

void PrivetNotificationsListener::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  DeviceContextMap::iterator found = devices_seen_.find(name);
  if (found != devices_seen_.end()) {
    if (!description.id.empty() &&  // Device is registered
        found->second->notification_may_be_active) {
      found->second->notification_may_be_active = false;
      delegate_->PrivetRemoveNotification(name);
    }
    return;  // Already saw this device.
  }

  linked_ptr<DeviceContext> device_context(new DeviceContext);

  device_context->notification_may_be_active = false;
  device_context->registered = !description.id.empty();
  device_context->human_readable_name = description.name;
  device_context->description = description.description;

  devices_seen_.insert(make_pair(name, device_context));

  if (!device_context->registered) {
    device_context->privet_http_resolution =
        privet_http_factory_->CreatePrivetHTTP(
            name,
            description.address,
            base::Bind(&PrivetNotificationsListener::CreateInfoOperation,
                       base::Unretained(this)));

    device_context->privet_http_resolution->Start();
  }
}

void PrivetNotificationsListener::CreateInfoOperation(
    scoped_ptr<PrivetHTTPClient> http_client) {
  std::string name = http_client->GetName();
  DeviceContextMap::iterator device_iter = devices_seen_.find(name);
  DCHECK(device_iter != devices_seen_.end());
  DeviceContext* device = device_iter->second.get();
  device->privet_http.swap(http_client);
  device->info_operation =
       device->privet_http->CreateInfoOperation(this);
  device->info_operation->Start();
}

void PrivetNotificationsListener::OnPrivetInfoDone(
      PrivetInfoOperation* operation,
      int http_code,
      const base::DictionaryValue* json_value) {
  std::string name = operation->GetHTTPClient()->GetName();
  DeviceContextMap::iterator device_iter = devices_seen_.find(name);
  DCHECK(device_iter != devices_seen_.end());
  DeviceContext* device = device_iter->second.get();

  int uptime;

  if (!json_value ||
      !json_value->GetInteger(kPrivetInfoKeyUptime, &uptime) ||
      uptime > kTenMinutesInSeconds) {
    return;
  }

  DCHECK(!device->notification_may_be_active);
  device->notification_may_be_active = true;
  delegate_->PrivetNotify(name, device->human_readable_name,
                          device->description);
}

void PrivetNotificationsListener::DeviceRemoved(const std::string& name) {
  DCHECK_EQ(1u, devices_seen_.count(name));
  DeviceContextMap::iterator device_iter = devices_seen_.find(name);
  DCHECK(device_iter != devices_seen_.end());
  DeviceContext* device = device_iter->second.get();

  device->info_operation.reset();
  device->privet_http_resolution.reset();
  device->notification_may_be_active = false;
  delegate_->PrivetRemoveNotification(name);
}

PrivetNotificationsListener::DeviceContext::DeviceContext() {
}

PrivetNotificationsListener::DeviceContext::~DeviceContext() {
}

PrivetNotificationService::PrivetNotificationService(
    content::BrowserContext* profile,
    NotificationUIManager* notification_manager)
    : profile_(profile),
      notification_manager_(notification_manager) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrivetNotificationService::Start, AsWeakPtr()));
}

PrivetNotificationService::~PrivetNotificationService() {
  ServiceDiscoveryHostClientFactory::ReleaseClient();
}

void PrivetNotificationService::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  privet_notifications_listener_->DeviceChanged(added, name, description);
}

void PrivetNotificationService::DeviceRemoved(const std::string& name) {
  privet_notifications_listener_->DeviceRemoved(name);
}

void PrivetNotificationService::PrivetNotify(
    const std::string& device_name,
    const std::string& human_readable_name,
    const std::string& description) {
  Profile* profile_object = Profile::FromBrowserContext(profile_);
  Notification notification(
      GURL(kPrivetNotificationOriginUrl),
      GURL(),
      l10n_util::GetStringUTF16(IDS_LOCAL_DISOCVERY_NOTIFICATION_TITLE_PRINTER),
      l10n_util::GetStringFUTF16(
          IDS_LOCAL_DISOCVERY_NOTIFICATION_CONTENTS_PRINTER,
          UTF8ToUTF16(human_readable_name)),
      WebKit::WebTextDirectionDefault,
      l10n_util::GetStringUTF16(
          IDS_LOCAL_DISOCVERY_NOTIFICATION_DISPLAY_SOURCE_PRINTER),
      UTF8ToUTF16(kPrivetNotificationIDPrefix +
                  device_name),
      new PrivetNotificationDelegate(device_name));

  notification_manager_->Add(notification, profile_object);
}

void PrivetNotificationService::PrivetRemoveNotification(
    const std::string& device_name) {
  notification_manager_->CancelById(kPrivetNotificationIDPrefix + device_name);
}

void PrivetNotificationService::Start() {
  service_discovery_client_ = ServiceDiscoveryHostClientFactory::GetClient();
  device_lister_.reset(new PrivetDeviceListerImpl(service_discovery_client_,
                                                  this));
  device_lister_->Start();

  scoped_ptr<PrivetHTTPAsynchronousFactory> http_factory(
      new PrivetHTTPAsynchronousFactoryImpl(service_discovery_client_.get(),
                                            profile_->GetRequestContext()));

  privet_notifications_listener_.reset(new PrivetNotificationsListener(
      http_factory.Pass(), this));
}

PrivetNotificationDelegate::PrivetNotificationDelegate(
    const std::string& device_id) : device_id_(device_id) {
}

PrivetNotificationDelegate::~PrivetNotificationDelegate() {
}

std::string PrivetNotificationDelegate::id() const {
  return kPrivetNotificationIDPrefix + device_id_;
}

content::RenderViewHost* PrivetNotificationDelegate::GetRenderViewHost() const {
  return NULL;
}

void PrivetNotificationDelegate::Display() {
}

void PrivetNotificationDelegate::Error() {
  LOG(ERROR) << "Error displaying privet notification " << device_id_;
}

void PrivetNotificationDelegate::Close(bool by_user) {
}

void PrivetNotificationDelegate::Click() {
}

}  // namespace local_discovery
