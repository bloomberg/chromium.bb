// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_constants.h"

#define FPL FILE_PATH_LITERAL

namespace chromeos {

const base::FilePath::CharType kDriveCacheDirname[] = FPL("GCache");
const char kOemDeviceRequisitionKey[] = "oem_device_requisition";
const char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";
const char kOemCanExitEnterpriseEnrollmentKey[] = "oem_can_exit_enrollment";
const char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";

}  // namespace chromeos
