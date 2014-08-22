// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_notifications.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notifier_settings.h"

#if defined(ENABLE_MDNS)
#include "chrome/browser/local_discovery/privet_traffic_detector.h"
#endif

namespace local_discovery {

namespace {

const int kTenMinutesInSeconds = 600;
const char kPrivetInfoKeyUptime[] = "uptime";
const char kPrivetNotificationID[] = "privet_notification";
const char kPrivetNotificationOriginUrl[] = "chrome://devices";
const int kStartDelaySeconds = 5;

enum PrivetNotificationsEvent {
  PRIVET_SERVICE_STARTED,
  PRIVET_LISTER_STARTED,
  PRIVET_DEVICE_CHANGED,
  PRIVET_INFO_DONE,
  PRIVET_NOTIFICATION_SHOWN,
  PRIVET_NOTIFICATION_CANCELED,
  PRIVET_NOTIFICATION_CLICKED,
  PRIVET_DISABLE_NOTIFICATIONS_CLICKED,
  PRIVET_EVENT_MAX,
};

void ReportPrivetUmaEvent(PrivetNotificationsEvent privet_event) {
  UMA_HISTOGRAM_ENUMERATION("LocalDiscovery.PrivetNotificationsEvent",
                            privet_event, PRIVET_EVENT_MAX);
}

}  // namespace

PrivetNotificationsListener::PrivetNotificationsListener(
    scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory,
    Delegate* delegate)
    : delegate_(delegate), devices_active_(0) {
  privet_http_factory_.swap(privet_http_factory);
}

PrivetNotificationsListener::~PrivetNotificationsListener() {
}

void PrivetNotificationsListener::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  ReportPrivetUmaEvent(PRIVET_DEVICE_CHANGED);
  DeviceContextMap::iterator found = devices_seen_.find(name);
  if (found != devices_seen_.end()) {
    if (!description.id.empty() &&  // Device is registered
        found->second->notification_may_be_active) {
      found->second->notification_may_be_active = false;
      NotifyDeviceRemoved();
    }
    return;  // Already saw this device.
  }

  linked_ptr<DeviceContext> device_context(new DeviceContext);

  device_context->notification_may_be_active = false;
  device_context->registered = !description.id.empty();

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
  if (!http_client) {
    // Do nothing if resolution fails.
    return;
  }

  std::string name = http_client->GetName();
  DeviceContextMap::iterator device_iter = devices_seen_.find(name);
  DCHECK(device_iter != devices_seen_.end());
  DeviceContext* device = device_iter->second.get();
  device->privet_http.swap(http_client);
  device->info_operation = device->privet_http->CreateInfoOperation(
      base::Bind(&PrivetNotificationsListener::OnPrivetInfoDone,
                 base::Unretained(this),
                 device));
  device->info_operation->Start();
}

void PrivetNotificationsListener::OnPrivetInfoDone(
    DeviceContext* device,
    const base::DictionaryValue* json_value) {
  int uptime;

  if (!json_value ||
      !json_value->GetInteger(kPrivetInfoKeyUptime, &uptime) ||
      uptime > kTenMinutesInSeconds) {
    return;
  }

  DCHECK(!device->notification_may_be_active);
  device->notification_may_be_active = true;
  devices_active_++;
  delegate_->PrivetNotify(devices_active_ > 1, true);
}

void PrivetNotificationsListener::DeviceRemoved(const std::string& name) {
  DCHECK_EQ(1u, devices_seen_.count(name));
  DeviceContextMap::iterator device_iter = devices_seen_.find(name);
  DCHECK(device_iter != devices_seen_.end());
  DeviceContext* device = device_iter->second.get();

  device->info_operation.reset();
  device->privet_http_resolution.reset();
  device->notification_may_be_active = false;
  NotifyDeviceRemoved();
}

void PrivetNotificationsListener::DeviceCacheFlushed() {
  for (DeviceContextMap::iterator i = devices_seen_.begin();
       i != devices_seen_.end(); ++i) {
    DeviceContext* device = i->second.get();

    device->info_operation.reset();
    device->privet_http_resolution.reset();
    if (device->notification_may_be_active) {
      device->notification_may_be_active = false;
    }
  }

  devices_active_ = 0;
  delegate_->PrivetRemoveNotification();
}

void PrivetNotificationsListener::NotifyDeviceRemoved() {
  devices_active_--;
  if (devices_active_ == 0) {
    delegate_->PrivetRemoveNotification();
  } else {
    delegate_->PrivetNotify(devices_active_ > 1, false);
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

// static
bool PrivetNotificationService::IsEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(
      switches::kDisableDeviceDiscoveryNotifications);
}

// static
bool PrivetNotificationService::IsForced() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableDeviceDiscoveryNotifications);
}

void PrivetNotificationService::PrivetNotify(bool has_multiple,
                                             bool added) {
  base::string16 product_name = l10n_util::GetStringUTF16(
      IDS_LOCAL_DISOCVERY_SERVICE_NAME_PRINTER);

  int title_resource = has_multiple ?
      IDS_LOCAL_DISOCVERY_NOTIFICATION_TITLE_PRINTER_MULTIPLE :
      IDS_LOCAL_DISOCVERY_NOTIFICATION_TITLE_PRINTER;

  int body_resource = has_multiple ?
      IDS_LOCAL_DISOCVERY_NOTIFICATION_CONTENTS_PRINTER_MULTIPLE :
      IDS_LOCAL_DISOCVERY_NOTIFICATION_CONTENTS_PRINTER;

  base::string16 title = l10n_util::GetStringUTF16(title_resource);
  base::string16 body = l10n_util::GetStringUTF16(body_resource);

  Profile* profile_object = Profile::FromBrowserContext(profile_);
  message_center::RichNotificationData rich_notification_data;

  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_LOCAL_DISOCVERY_NOTIFICATION_BUTTON_PRINTER)));

  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_LOCAL_DISCOVERY_NOTIFICATIONS_DISABLE_BUTTON_LABEL)));

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(kPrivetNotificationOriginUrl),
      title,
      body,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_LOCAL_DISCOVERY_CLOUDPRINT_ICON),
      blink::WebTextDirectionDefault,
      message_center::NotifierId(GURL(kPrivetNotificationOriginUrl)),
      product_name,
      base::UTF8ToUTF16(kPrivetNotificationID),
      rich_notification_data,
      new PrivetNotificationDelegate(profile_));

  bool updated = g_browser_process->notification_ui_manager()->Update(
      notification, profile_object);
  if (!updated && added && !LocalDiscoveryUIHandler::GetHasVisible()) {
    ReportPrivetUmaEvent(PRIVET_NOTIFICATION_SHOWN);
    g_browser_process->notification_ui_manager()->Add(notification,
                                                      profile_object);
  }
}

void PrivetNotificationService::PrivetRemoveNotification() {
  ReportPrivetUmaEvent(PRIVET_NOTIFICATION_CANCELED);
  g_browser_process->notification_ui_manager()->CancelById(
      kPrivetNotificationID);
}

void PrivetNotificationService::Start() {
#if defined(CHROMEOS)
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(
          Profile::FromBrowserContext(profile_));

  if (!signin_manager || signin_manager->GetAuthenticatedUsername().empty())
    return;
#endif

  enable_privet_notification_member_.Init(
      prefs::kLocalDiscoveryNotificationsEnabled,
      Profile::FromBrowserContext(profile_)->GetPrefs(),
      base::Bind(&PrivetNotificationService::OnNotificationsEnabledChanged,
                 base::Unretained(this)));
  OnNotificationsEnabledChanged();
}

void PrivetNotificationService::OnNotificationsEnabledChanged() {
#if defined(ENABLE_MDNS)
  if (IsForced()) {
    StartLister();
  } else if (*enable_privet_notification_member_) {
    ReportPrivetUmaEvent(PRIVET_SERVICE_STARTED);
    traffic_detector_ =
        new PrivetTrafficDetector(
            net::ADDRESS_FAMILY_IPV4,
            base::Bind(&PrivetNotificationService::StartLister, AsWeakPtr()));
    traffic_detector_->Start();
  } else {
    traffic_detector_ = NULL;
    device_lister_.reset();
    service_discovery_client_ = NULL;
    privet_notifications_listener_.reset();
  }
#else
  if (IsForced() || *enable_privet_notification_member_) {
    StartLister();
  } else {
    device_lister_.reset();
    service_discovery_client_ = NULL;
    privet_notifications_listener_.reset();
  }
#endif
}

void PrivetNotificationService::StartLister() {
  ReportPrivetUmaEvent(PRIVET_LISTER_STARTED);
#if defined(ENABLE_MDNS)
  traffic_detector_ = NULL;
#endif  // ENABLE_MDNS
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
    content::BrowserContext* profile)
    :  profile_(profile) {
}

PrivetNotificationDelegate::~PrivetNotificationDelegate() {
}

std::string PrivetNotificationDelegate::id() const {
  return kPrivetNotificationID;
}

content::WebContents* PrivetNotificationDelegate::GetWebContents() const {
  return NULL;
}

void PrivetNotificationDelegate::Display() {
}

void PrivetNotificationDelegate::Error() {
  LOG(ERROR) << "Error displaying privet notification";
}

void PrivetNotificationDelegate::Close(bool by_user) {
}

void PrivetNotificationDelegate::Click() {
}

void PrivetNotificationDelegate::ButtonClick(int button_index) {
  if (button_index == 0) {
    ReportPrivetUmaEvent(PRIVET_NOTIFICATION_CLICKED);
    OpenTab(GURL(kPrivetNotificationOriginUrl));
  } else if (button_index == 1) {
    ReportPrivetUmaEvent(PRIVET_DISABLE_NOTIFICATIONS_CLICKED);
    DisableNotifications();
  }
}

void PrivetNotificationDelegate::OpenTab(const GURL& url) {
  Profile* profile_obj = Profile::FromBrowserContext(profile_);

  chrome::NavigateParams params(profile_obj,
                              url,
                              content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void PrivetNotificationDelegate::DisableNotifications() {
  Profile* profile_obj = Profile::FromBrowserContext(profile_);

  profile_obj->GetPrefs()->SetBoolean(
      prefs::kLocalDiscoveryNotificationsEnabled,
      false);
}

}  // namespace local_discovery
