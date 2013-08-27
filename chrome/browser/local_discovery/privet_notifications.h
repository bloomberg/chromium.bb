// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "ui/message_center/notification_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace local_discovery {

// Contains logic related to notifications not tied actually displaying them.
class PrivetNotificationsListener : public PrivetInfoOperation::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notify user of the existence of device |device_name|.
    virtual void PrivetNotify(const std::string& device_name,
                              const std::string& human_readable_name,
                              const std::string& description) = 0;

    // Remove the noitification for |device_name| if it still exists.
    virtual void PrivetRemoveNotification(const std::string& device_name) = 0;
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

  // PrivetInfoOperation::Delegate implementation.
  virtual void OnPrivetInfoDone(
      PrivetInfoOperation* operation,
      int http_code,
      const base::DictionaryValue* json_value) OVERRIDE;

 private:
  struct DeviceContext {
    DeviceContext();
    ~DeviceContext();

    bool notification_may_be_active;
    bool registered;
    std::string human_readable_name;
    std::string description;
    scoped_ptr<PrivetInfoOperation> info_operation;
    scoped_ptr<PrivetHTTPAsynchronousFactory::Resolution>
        privet_http_resolution;
    scoped_ptr<PrivetHTTPClient> privet_http;
  };

  typedef std::map<std::string, linked_ptr<DeviceContext> > DeviceContextMap;

  void CreateInfoOperation(scoped_ptr<PrivetHTTPClient> http_client);

  Delegate* delegate_;
  scoped_ptr<PrivetDeviceLister> device_lister_;
  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;
  DeviceContextMap devices_seen_;
};

class PrivetNotificationService
    : public BrowserContextKeyedService,
      public PrivetDeviceLister::Delegate,
      public PrivetNotificationsListener::Delegate,
      public base::SupportsWeakPtr<PrivetNotificationService> {
 public:
  PrivetNotificationService(content::BrowserContext* profile,
                            NotificationUIManager* notification_manager);
  virtual ~PrivetNotificationService();

  // PrivetDeviceLister::Delegate implementation:
  virtual void DeviceChanged(bool added, const std::string& name,
                             const DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;

  // PrivetNotificationListener::Delegate implementation:
  virtual void PrivetNotify(const std::string& device_name,
                            const std::string& human_readable_name,
                            const std::string& description) OVERRIDE;

  virtual void PrivetRemoveNotification(
      const std::string& device_name) OVERRIDE;
 private:
  void Start();

  content::BrowserContext* profile_;
  NotificationUIManager* notification_manager_;
  scoped_ptr<PrivetDeviceLister> device_lister_;
  scoped_refptr<ServiceDiscoveryHostClient> service_discovery_client_;
  scoped_ptr<PrivetNotificationsListener> privet_notifications_listener_;
};

class PrivetNotificationDelegate : public NotificationDelegate {
 public:
  explicit PrivetNotificationDelegate(const std::string& device_id,
                                      content::BrowserContext* profile);

  // NotificationDelegate implementation.
  virtual std::string id() const OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;

 private:
  void OpenTab(const GURL& url);

  virtual ~PrivetNotificationDelegate();

  std::string device_id_;
  content::BrowserContext* profile_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_H_
