// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_VIRTUAL_DRIVER_INSTALL_HELPER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_VIRTUAL_DRIVER_INSTALL_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"

namespace cloud_print {

// Helper class to register the Cloud Print Driver with the service process.
class VirtualDriverInstallHelper
    : public base::RefCountedThreadSafe<VirtualDriverInstallHelper> {
 public:
  // Uses ServiceProcessControl to asynchronously get an instance
  // of the Service Process, launching it if necessary, and register
  // either InstallVirtualDriverTask or UninstallVirtualDriverTask as the
  // Task to be called once we have an instance of the ServiceProcess.
  static void SetUpInstall();
  static void SetUpUninstall();

 private:
  friend class base::RefCountedThreadSafe<VirtualDriverInstallHelper>;

  ~VirtualDriverInstallHelper() {}

  void InstallVirtualDriverTask();
  void UninstallVirtualDriverTask();
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_VIRTUAL_DRIVER_INSTALL_HELPER_H_

