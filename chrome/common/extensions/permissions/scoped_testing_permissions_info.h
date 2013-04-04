// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SCOPED_TESTING_PERMISSIONS_INFO_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SCOPED_TESTING_PERMISSIONS_INFO_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/permissions/permissions_info.h"

namespace extensions {

// Overrides the global PermissionsInfo on construction and restores it
// on destruction.
class ScopedTestingPermissionsInfo {
 public:
  ScopedTestingPermissionsInfo();
  explicit ScopedTestingPermissionsInfo(
      const PermissionsInfo::Delegate& delegate);
  ~ScopedTestingPermissionsInfo();

 private:
  scoped_ptr<PermissionsInfo> info_;
  PermissionsInfo* old_info_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SCOPED_TESTING_PERMISSIONS_INFO_H_
