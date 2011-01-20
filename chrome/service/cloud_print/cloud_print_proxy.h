// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

class ServiceProcessPrefs;

// CloudPrintProxy is the layer between the service process UI thread
// and the cloud print proxy backend.
class CloudPrintProxy : public CloudPrintProxyFrontend,
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
  void DisableForUser();
  // Returns the enabled stata of the proxy. If enabled, the email address used
  // for authentication is also returned in the optional |email| argument.
  bool IsEnabled(std::string* email) const;

  const std::string& cloud_print_email() const {
    return cloud_print_email_;
  }

  // CloudPrintProxyFrontend implementation. Called on UI thread.
  virtual void OnPrinterListAvailable(
      const printing::PrinterList& printer_list);
  virtual void OnAuthenticated(const std::string& cloud_print_token,
                               const std::string& cloud_print_xmpp_token,
                               const std::string& email);
  virtual void OnAuthenticationFailed();
  virtual void OnPrintSystemUnavailable();

 protected:
  void Shutdown();

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
  std::string cloud_print_email_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxy);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
