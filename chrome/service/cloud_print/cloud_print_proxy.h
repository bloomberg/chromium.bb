// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

// CloudPrintProxy is the layer between the service process UI thread
// and the cloud print proxy backend.
class CloudPrintProxy : public CloudPrintProxyFrontend {
 public:
  explicit CloudPrintProxy();
  virtual ~CloudPrintProxy();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& lsid,
                             const std::string& proxy_id);
  virtual void DisableForUser();

  // Notification received from the server for a particular printer.
  // We need to inform the backend to look for jobs for this printer.
  void HandlePrinterNotification(const std::string& printer_id);

  // Notification methods from the backend. Called on UI thread.
  void OnPrinterListAvailable(const cloud_print::PrinterList& printer_list);

 protected:
  void Shutdown();

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<CloudPrintProxyBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxy);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_

