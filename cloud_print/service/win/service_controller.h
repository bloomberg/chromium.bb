// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_
#define CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_

#include <atlbase.h>
#include <string>

#include "base/string16.h"
#include "cloud_print/service/win/resource.h"

namespace base {
class FilePath;
}  // base

class ServiceController {
 public:
  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                  "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")

  explicit ServiceController(const string16& name);
  ~ServiceController();

  HRESULT InstallService(const string16& user,
                         const string16& password,
                         const std::string& run_switch,
                         const base::FilePath& user_data_dir,
                         bool auto_start);

  HRESULT UninstallService();

  HRESULT StartService();
  HRESULT StopService();

 private:
  const string16 name_;
};

#endif  // CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_

