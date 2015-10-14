// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_
#define CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_

#include <atlbase.h>
#include <string>

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "cloud_print/resources.h"

namespace base {
class FilePath;
}  // base

class ServiceController {
 public:
  enum State {
    STATE_UNKNOWN = 0,
    STATE_NOT_FOUND,
    STATE_STOPPED,
    STATE_RUNNING,
  };

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                    "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")

  ServiceController();
  ~ServiceController();

  // Installs temporarily service to check pre-requirements.
  HRESULT InstallCheckService(const base::string16& user,
                              const base::string16& password,
                              const base::FilePath& user_data_dir);

  // Installs real service that will run connector.
  HRESULT InstallConnectorService(const base::string16& user,
                                  const base::string16& password,
                                  const base::FilePath& user_data_dir,
                                  bool enable_logging);

  HRESULT UninstallService();

  HRESULT StartService();
  HRESULT StopService();

  HRESULT UpdateBinaryPath();

  // Query service status and options. Results accessible with getters below.
  void UpdateState();
  State state() const { return state_; }
  const base::string16& user() const { return user_; }
  bool is_logging_enabled() const;

  base::FilePath GetBinary() const;

 private:
  HRESULT InstallService(const base::string16& user,
                         const base::string16& password,
                         bool auto_start,
                         const std::string& run_switch,
                         const base::FilePath& user_data_dir,
                         bool enable_logging);

  const base::string16 name_;
  State state_;
  base::string16 user_;
  bool is_logging_enabled_;
  base::CommandLine command_line_;
};

#endif  // CLOUD_PRINT_SERVICE_SERVICE_CONTROLLER_H_

