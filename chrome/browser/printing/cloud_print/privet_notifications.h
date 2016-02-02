// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_NOTIFICATIONS_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_NOTIFICATIONS_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"

class NotificationUIManager;

namespace content {
class BrowserContext;
}

namespace local_discovery {
class ServiceDiscoverySharedClient;
}

namespace cloud_print {

class PrivetDeviceLister;
class PrivetHTTPAsynchronousFactory;
class PrivetHTTPResolution;
struct DeviceDescription;

#if defined(ENABLE_MDNS)
class PrivetTrafficDetector;
#endif  // ENABLE_MDNS

// Contains logic related to notifications not tied actually displaying them.
class PrivetNotificationsListener  {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notify user of the existence of device |device_name|.
    virtual void PrivetNotify(int devices_active, bool added) = 0;

    // Remove the noitification for |device_name| if it still exists.
    virtual void PrivetRemoveNotification() = 0;
  };

  PrivetNotificationsListener(
      scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory,
      Delegate* delegate);
  virtual ~PrivetNotificationsListener();

  // These two methods are akin to those of PrivetDeviceLister::Delegate. The
  // user of PrivetNotificationListener should create a PrivetDeviceLister and
  // forward device notifications to the PrivetNotificationLister.
  void DeviceChanged(bool added,
                     const std::string& name,
                     const DeviceDescription& description);
  void DeviceRemoved(const std::string& name);
  virtual void DeviceCacheFlushed();

 private:
  struct DeviceContext {
    DeviceContext();
    ~DeviceContext();

    bool notification_may_be_active;
    bool registered;
    scoped_ptr<PrivetJSONOperation> info_operation;
    scoped_ptr<PrivetHTTPResolution> privet_http_resolution;
    scoped_ptr<PrivetHTTPClient> privet_http;
  };

  using DeviceContextMap = std::map<std::string, scoped_ptr<DeviceContext>>;

  void CreateInfoOperation(scoped_ptr<PrivetHTTPClient> http_client);
  void OnPrivetInfoDone(DeviceContext* device,
                        const base::DictionaryValue* json_value);


  void NotifyDeviceRemoved();

  Delegate* delegate_;
  scoped_ptr<PrivetDeviceLister> device_lister_;
  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;
  DeviceContextMap devices_seen_;
  int devices_active_;
};

class PrivetNotificationService
    : public KeyedService,
      public PrivetDeviceLister::Delegate,
      public PrivetNotificationsListener::Delegate,
      public base::SupportsWeakPtr<PrivetNotificationService> {
 public:
  explicit PrivetNotificationService(content::BrowserContext* profile);
  ~PrivetNotificationService() override;

  // PrivetDeviceLister::Delegate implementation:
  void DeviceChanged(bool added,
                     const std::string& name,
                     const DeviceDescription& description) override;
  void DeviceRemoved(const std::string& name) override;

  // PrivetNotificationListener::Delegate implementation:
  void PrivetNotify(int devices_active, bool added) override;

  void PrivetRemoveNotification() override;
  void DeviceCacheFlushed() override;

  static bool IsEnabled();
  static bool IsForced();

 private:
  void Start();
  void OnNotificationsEnabledChanged();
  void StartLister();

  content::BrowserContext* profile_;
  scoped_ptr<PrivetDeviceLister> device_lister_;
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  scoped_ptr<PrivetNotificationsListener> privet_notifications_listener_;
  BooleanPrefMember enable_privet_notification_member_;

#if defined(ENABLE_MDNS)
  scoped_refptr<PrivetTrafficDetector> traffic_detector_;
#endif  // ENABLE_MDNS
};

class PrivetNotificationDelegate : public NotificationDelegate {
 public:
  explicit PrivetNotificationDelegate(content::BrowserContext* profile);

  // NotificationDelegate implementation.
  std::string id() const override;
  void ButtonClick(int button_index) override;

 private:
  void OpenTab(const GURL& url);
  void DisableNotifications();

  ~PrivetNotificationDelegate() override;

  content::BrowserContext* profile_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_NOTIFICATIONS_H_
