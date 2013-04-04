// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/scoped_testing_permissions_info.h"

namespace extensions {

ScopedTestingPermissionsInfo::ScopedTestingPermissionsInfo()
    : info_(new PermissionsInfo),
      old_info_(PermissionsInfo::GetInstance()) {
  PermissionsInfo::SetForTesting(info_.get());
}

ScopedTestingPermissionsInfo::ScopedTestingPermissionsInfo(
    const PermissionsInfo::Delegate& delegate)
    : info_(new PermissionsInfo),
      old_info_(PermissionsInfo::GetInstance()) {
  PermissionsInfo::SetForTesting(info_.get());
  PermissionsInfo::GetInstance()->InitializeWithDelegate(delegate);
}

ScopedTestingPermissionsInfo::~ScopedTestingPermissionsInfo() {
  PermissionsInfo::SetForTesting(old_info_);
}

}  // namespace extensions
