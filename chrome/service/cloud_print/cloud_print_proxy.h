// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#pragma once

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"
#include "chrome/service/cloud_print/cloud_print_wipeout.h"

class ServiceProcessPrefs;

namespace cloud_print {
struct CloudPrintProxyInfo;
}  // namespace cloud_print

// CloudPrintProxy is the layer between the service process UI thread
// and the cloud print proxy backend.
class CloudPrintProxy : public CloudPrintProxyFrontend,
                        public CloudPrintWipeout::Client,
                        public base::NonThreadSafe {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void OnCloudPrintProxyEnabled(bool persist_state) {}
    virtual void OnCloudPrintProxyDisabled(bool persist_state) {}
  };
  CloudPrintProxy();
  virtual ~CloudPrintProxy();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize(ServiceProcessPrefs* service_prefs, Client* client);

  // Enables/disables cloud printing for the user
  void EnableForUser(const std::string& lsid);
  void EnableForUserWithRobot(
      const std::string& robot_auth_code,
      const std::string& robot_email,
      const std::string& user_email);
  void UnregisterPrintersAndDisableForUser();
  void DisableForUser();
  // Returns the proxy info.
  void GetProxyInfo(cloud_print::CloudPrintProxyInfo* info);

  // Launches a browser to see if the proxy policy has been set.
  void CheckCloudPrintProxyPolicy();

  const std::string& user_email() const {
    return user_email_;
  }

  // CloudPrintProxyFrontend implementation. Called on UI thread.
  virtual void OnAuthenticated(const std::string& robot_oauth_refresh_token,
                               const std::string& robot_email,
                               const std::string& user_email) OVERRIDE;
  virtual void OnAuthenticationFailed() OVERRIDE;
  virtual void OnPrintSystemUnavailable() OVERRIDE;
  virtual void OnUnregisterPrinters(
      const std::string& auth_token,
      const std::list<std::string> printer_ids) OVERRIDE;

  // CloudPrintWipeout::Client implementation.
  virtual void OnUnregisterPrintersComplete() OVERRIDE;

 protected:
  void ShutdownBackend();
  bool CreateBackend();

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<CloudPrintProxyBackend> backend_;
  // This class does not own this. It is guaranteed to remain valid for the
  // lifetime of this class.
  ServiceProcessPrefs* service_prefs_;
  // This class does not own this. If non-NULL, It is guaranteed to remain
  // valid for the lifetime of this class.
  Client* client_;
  // The email address of the account used to authenticate to the Cloud Print
  // service.
  std::string user_email_;
  // This is set to true when the Cloud Print proxy is enabled and after
  // successful authentication with the Cloud Print service.
  bool enabled_;
  // This is initialized after a successful call to one of the Enable* methods.
  // It is not cleared in DisableUser.
  std::string proxy_id_;
  // Cloud Print server url.
  GURL cloud_print_server_url_;
  // This is a cleanup class for unregistering printers on proxy disable.
  scoped_ptr<CloudPrintWipeout> wipeout_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxy);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
