// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

class JsonPrefStore;

// CloudPrintProxy is the layer between the service process UI thread
// and the cloud print proxy backend.
class CloudPrintProxy : public CloudPrintProxyFrontend,
                        public NonThreadSafe {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void OnCloudPrintProxyEnabled() {}
    virtual void OnCloudPrintProxyDisabled() {}
  };
  CloudPrintProxy();
  virtual ~CloudPrintProxy();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize(JsonPrefStore* service_prefs, Client* client);

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& lsid);
  virtual void DisableForUser();

  // Notification methods from the backend. Called on UI thread.
  virtual void OnPrinterListAvailable(
      const cloud_print::PrinterList& printer_list);
  virtual void OnAuthenticated(const std::string& cloud_print_token,
                               const std::string& cloud_print_xmpp_token,
                               const std::string& email);

 protected:
  void Shutdown();

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<CloudPrintProxyBackend> backend_;
  // This class does not own this. It is guaranteed to remain valid for the
  // lifetime of this class.
  JsonPrefStore* service_prefs_;
  // This class does not own this. If non-NULL, It is guaranteed to remain
  // valid for the lifetime of this class.
  Client* client_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxy);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_

