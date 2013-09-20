// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_notifications.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/privet_traffic_detector.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notifier_settings.h"

namespace local_discovery {

namespace {
const int kTenMinutesInSeconds = 600;
const char kPrivetInfoKeyUptime[] = "uptime";
const char kPrivetNotificationIDPrefix[] = "privet_notification:";
const char kPrivetNotificationOriginUrl[] = "chrome://devices";
const int kStartDelaySeconds = 5;
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

void PrivetNotificationsListener::DeviceCacheFlushed() {
  for (DeviceContextMap::iterator i = devices_seen_.begin();
       i != devices_seen_.end(); ++i) {
    DeviceContext* device = i->second.get();

    device->info_operation.reset();
    device->privet_http_resolution.reset();
    if (device->notification_may_be_active) {
      device->notification_may_be_active = false;
      delegate_->PrivetRemoveNotification(i->first);
    }
  }
}

PrivetNotificationsListener::DeviceContext::DeviceContext() {
}

PrivetNotificationsListener::DeviceContext::~DeviceContext() {
}

PrivetNotificationService::PrivetNotificationService(
    content::BrowserContext* profile)
    : profile_(profile) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrivetNotificationService::Start, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kStartDelaySeconds +
                                   base::RandInt(0, kStartDelaySeconds/4)));
}

PrivetNotificationService::~PrivetNotificationService() {
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

void PrivetNotificationService::DeviceCacheFlushed() {
  privet_notifications_listener_->DeviceCacheFlushed();
}

void PrivetNotificationService::PrivetNotify(
    const std::string& device_name,
    const std::string& human_readable_name,
    const std::string& description) {
  if (!LocalDiscoveryUIHandler::GetHasVisible()) {
    Profile* profile_object = Profile::FromBrowserContext(profile_);
    message_center::RichNotificationData rich_notification_data;

    rich_notification_data.buttons.push_back(
        message_center::ButtonInfo(l10n_util::GetStringUTF16(
            IDS_LOCAL_DISOCVERY_NOTIFICATION_BUTTON_PRINTER)));

    Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GURL(kPrivetNotificationOriginUrl),
        l10n_util::GetStringUTF16(
            IDS_LOCAL_DISOCVERY_NOTIFICATION_TITLE_PRINTER),
        l10n_util::GetStringFUTF16(
            IDS_LOCAL_DISOCVERY_NOTIFICATION_CONTENTS_PRINTER,
            UTF8ToUTF16(human_readable_name)),
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_LOCAL_DISCOVERY_CLOUDPRINT_ICON),
        WebKit::WebTextDirectionDefault,
        message_center::NotifierId(
            message_center::NotifierId::SYSTEM_COMPONENT),
        l10n_util::GetStringUTF16(
            IDS_LOCAL_DISOCVERY_NOTIFICATION_DISPLAY_SOURCE_PRINTER),
        UTF8ToUTF16(kPrivetNotificationIDPrefix +
                    device_name),
        rich_notification_data,
        new PrivetNotificationDelegate(device_name, profile_));

    g_browser_process->notification_ui_manager()->Add(notification,
                                                      profile_object);
  }
}

void PrivetNotificationService::PrivetRemoveNotification(
    const std::string& device_name) {
  g_browser_process->notification_ui_manager()->CancelById(
      kPrivetNotificationIDPrefix + device_name);
}

void PrivetNotificationService::Start() {
  traffic_detector_v4_ =
      new PrivetTrafficDetector(
          net::ADDRESS_FAMILY_IPV4,
          base::Bind(&PrivetNotificationService::StartLister, AsWeakPtr()));
  traffic_detector_v6_ =
      new PrivetTrafficDetector(
          net::ADDRESS_FAMILY_IPV6,
          base::Bind(&PrivetNotificationService::StartLister, AsWeakPtr()));
}

void PrivetNotificationService::StartLister() {
  traffic_detector_v4_ = NULL;
  traffic_detector_v6_ = NULL;
  DCHECK(!service_discovery_client_);
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
  device_lister_.reset(new PrivetDeviceListerImpl(service_discovery_client_,
                                                  this));
  device_lister_->Start();
  device_lister_->DiscoverNewDevices(false);

  scoped_ptr<PrivetHTTPAsynchronousFactory> http_factory(
      PrivetHTTPAsynchronousFactory::CreateInstance(
          service_discovery_client_.get(), profile_->GetRequestContext()));

  privet_notifications_listener_.reset(new PrivetNotificationsListener(
      http_factory.Pass(), this));
}

PrivetNotificationDelegate::PrivetNotificationDelegate(
    const std::string& device_id, content::BrowserContext* profile)
    : device_id_(device_id), profile_(profile) {
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

void PrivetNotificationDelegate::ButtonClick(int button_index) {
  if (button_index == 0) {
    // TODO(noamsml): Direct-to-register URL
    OpenTab(GURL(kPrivetNotificationOriginUrl));
  }
}

void PrivetNotificationDelegate::OpenTab(const GURL& url) {
  Profile* profile_obj = Profile::FromBrowserContext(profile_);

  Browser* browser = FindOrCreateTabbedBrowser(profile_obj,
                                               chrome::GetActiveDesktop());
  content::WebContents::CreateParams create_params(profile_obj);

  scoped_ptr<content::WebContents> contents(
      content::WebContents::Create(create_params));
  contents->GetController().LoadURL(url,
                                    content::Referrer(),
                                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    "");

  browser->tab_strip_model()->AppendWebContents(contents.release(), true);
}


}  // namespace local_discovery
