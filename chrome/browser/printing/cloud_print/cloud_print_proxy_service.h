// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/profile.h"

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_backend.h"

class Profile;

// Layer between the browser user interface and the cloud print proxy backend.
class CloudPrintProxyService : public CloudPrintProxyFrontend {
 public:
  explicit CloudPrintProxyService(Profile* profile);
  virtual ~CloudPrintProxyService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& auth_token);
  virtual void DisableForUser();
  // Notification received from the server for a particular printer.
  // We need to inform the backend to look for jobs for this printer.
  void HandlePrinterNotification(const std::string& printer_id);

  // Notification methods from the backend. Called on UI thread.
  void OnPrinterListAvailable(const cloud_print::PrinterList& printer_list);

  // Called when authentication is done. Called on UI thread.
  // Note that sid can be empty. This is placeholder code until we can get
  // common GAIA signin code in place.
  // TODO(sanjeevr): Remove this.
  static void OnAuthenticated(const std::string& auth_token);

 protected:
  void Shutdown();

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<CloudPrintProxyBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyService);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_

