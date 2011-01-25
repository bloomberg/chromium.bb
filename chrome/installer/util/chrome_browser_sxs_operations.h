// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CHROME_BROWSER_SXS_OPERATIONS_H_
#define CHROME_INSTALLER_UTIL_CHROME_BROWSER_SXS_OPERATIONS_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/installer/util/chrome_browser_operations.h"

namespace installer {

// Operations specific to Chrome SxS; see ProductOperations for general info.
class ChromeBrowserSxSOperations : public ChromeBrowserOperations {
 public:
  ChromeBrowserSxSOperations() {}

  virtual void AppendProductFlags(
      const std::set<std::wstring>& options,
      CommandLine* uninstall_command) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserSxSOperations);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_CHROME_BROWSER_SXS_OPERATIONS_H_
