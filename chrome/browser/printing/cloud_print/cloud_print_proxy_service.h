// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"

class Profile;

// Layer between the browser user interface and the cloud print proxy code
// running in the service process.
class CloudPrintProxyService
    : public CloudPrintSetupHandler::Delegate,
      public base::RefCountedThreadSafe<CloudPrintProxyService> {
 public:
  explicit CloudPrintProxyService(Profile* profile);
  virtual ~CloudPrintProxyService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& lsid, const std::string& email);
  virtual void DisableForUser();

  // Query the service process for the status of the cloud print proxy and
  // update the browser prefs.
  void RefreshStatusFromService();

  bool ShowTokenExpiredNotification();

  // CloudPrintSetupHandler::Delegate implementation.
  virtual void OnCloudPrintSetupClosed();

 private:
  // NotificationDelegate implementation for the token expired notification.
  class TokenExpiredNotificationDelegate;
  friend class TokenExpiredNotificationDelegate;

  Profile* profile_;
  scoped_refptr<TokenExpiredNotificationDelegate> token_expired_delegate_;
  scoped_ptr<CloudPrintSetupHandler> cloud_print_setup_handler_;

  // Methods that send an IPC to the service.
  void RefreshCloudPrintProxyStatus();
  void EnableCloudPrintProxy(const std::string& lsid, const std::string& email);
  void DisableCloudPrintProxy();

  // Callback that gets the cloud print proxy status.
  void StatusCallback(bool enabled, std::string email);
  // Invoke a task that gets run after the service process successfully
  // launches. The task typically involves sending an IPC to the service
  // process.
  bool InvokeServiceTask(Task* task);

  void OnTokenExpiredNotificationError();
  void OnTokenExpiredNotificationClosed(bool by_user);
  void OnTokenExpiredNotificationClick();
  void TokenExpiredNotificationDone(bool keep_alive);

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyService);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
