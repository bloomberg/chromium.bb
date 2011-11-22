// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/virtual_driver_install_helper.h"

#include "base/bind.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/common/service_messages.h"

namespace cloud_print {

void VirtualDriverInstallHelper::SetUpInstall() {
  scoped_refptr<VirtualDriverInstallHelper> help =
      new VirtualDriverInstallHelper();
  ServiceProcessControl::GetInstance()->Launch(
      base::Bind(&VirtualDriverInstallHelper::InstallVirtualDriverTask,
                 help.get()),
      base::Closure());
}

void VirtualDriverInstallHelper::SetUpUninstall() {
  scoped_refptr<VirtualDriverInstallHelper> help =
      new VirtualDriverInstallHelper();
  ServiceProcessControl::GetInstance()->Launch(
      base::Bind(&VirtualDriverInstallHelper::UninstallVirtualDriverTask,
                 help.get()),
      base::Closure());
}

void VirtualDriverInstallHelper::InstallVirtualDriverTask() {
  ServiceProcessControl* process_control =
      ServiceProcessControl::GetInstance();
  DCHECK(process_control->IsConnected());
  process_control->Send(new ServiceMsg_EnableVirtualDriver());
}

void VirtualDriverInstallHelper::UninstallVirtualDriverTask() {
  ServiceProcessControl* process_control =
      ServiceProcessControl::GetInstance();
  DCHECK(process_control->IsConnected());
  process_control->Send(new ServiceMsg_DisableVirtualDriver());
}

}  // namespace cloud_print
