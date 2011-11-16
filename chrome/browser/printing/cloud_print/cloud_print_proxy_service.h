// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"

class Profile;
class ServiceProcessControl;

namespace cloud_print {
struct CloudPrintProxyInfo;
}  // namespace cloud_print

// Layer between the browser user interface and the cloud print proxy code
// running in the service process.
class CloudPrintProxyService
    : public CloudPrintSetupHandlerDelegate,
      public ProfileKeyedService,
      public content::NotificationObserver {
 public:
  explicit CloudPrintProxyService(Profile* profile);
  virtual ~CloudPrintProxyService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& lsid, const std::string& email);
  virtual void EnableForUserWithRobot(const std::string& robot_auth_code,
                                      const std::string& robot_email,
                                      const std::string& user_email);
  virtual void DisableForUser();

  // Query the service process for the status of the cloud print proxy and
  // update the browser prefs.
  void RefreshStatusFromService();

  bool ShowTokenExpiredNotification();
  std::string proxy_id() const { return proxy_id_; }

  // CloudPrintSetupHandler::Delegate implementation.
  virtual void OnCloudPrintSetupClosed();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // NotificationDelegate implementation for the token expired notification.
  class TokenExpiredNotificationDelegate;
  friend class TokenExpiredNotificationDelegate;

  Profile* profile_;
  scoped_refptr<TokenExpiredNotificationDelegate> token_expired_delegate_;
  scoped_ptr<CloudPrintSetupHandler> cloud_print_setup_handler_;
  std::string proxy_id_;

  // Methods that send an IPC to the service.
  void RefreshCloudPrintProxyStatus();
  void EnableCloudPrintProxy(const std::string& lsid, const std::string& email);
  void EnableCloudPrintProxyWithRobot(const std::string& robot_auth_code,
                                      const std::string& robot_email,
                                      const std::string& user_email);
  void DisableCloudPrintProxy();

  // Callback that gets the cloud print proxy info.
  void ProxyInfoCallback(
    const cloud_print::CloudPrintProxyInfo& proxy_info);

  // Invoke a task that gets run after the service process successfully
  // launches. The task typically involves sending an IPC to the service
  // process.
  bool InvokeServiceTask(const base::Closure& task);

  void OnTokenExpiredNotificationError();
  void OnTokenExpiredNotificationClosed(bool by_user);
  void OnTokenExpiredNotificationClick();
  void TokenExpiredNotificationDone(bool keep_alive);
  void ApplyCloudPrintConnectorPolicy();

  // Virtual for testing.
  virtual ServiceProcessControl* GetServiceProcessControl();

  base::WeakPtrFactory<CloudPrintProxyService> weak_factory_;

  // For watching for connector enablement policy changes.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyService);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
