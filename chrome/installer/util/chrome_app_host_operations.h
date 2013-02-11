// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CHROME_APP_HOST_OPERATIONS_H_
#define CHROME_INSTALLER_UTIL_CHROME_APP_HOST_OPERATIONS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/installer/util/product_operations.h"

namespace installer {

// Operations specific to Chrome App Host; see ProductOperations for general
// info.
class ChromeAppHostOperations : public ProductOperations {
 public:
  ChromeAppHostOperations() {}

  virtual void ReadOptions(const MasterPreferences& prefs,
                           std::set<std::wstring>* options) const OVERRIDE;

  virtual void ReadOptions(const CommandLine& uninstall_command,
                           std::set<std::wstring>* options) const OVERRIDE;

  virtual void AddKeyFiles(
      const std::set<std::wstring>& options,
      std::vector<base::FilePath>* key_files) const OVERRIDE;

  virtual void AddComDllList(
      const std::set<std::wstring>& options,
      std::vector<base::FilePath>* com_dll_list) const OVERRIDE;

  virtual void AppendProductFlags(
      const std::set<std::wstring>& options,
      CommandLine* cmd_line) const OVERRIDE;

  virtual void AppendRenameFlags(
      const std::set<std::wstring>& options,
      CommandLine* cmd_line) const OVERRIDE;

  virtual bool SetChannelFlags(const std::set<std::wstring>& options,
                               bool set,
                               ChannelInfo* channel_info) const OVERRIDE;

  virtual bool ShouldCreateUninstallEntry(
      const std::set<std::wstring>& options) const OVERRIDE;

  virtual void AddDefaultShortcutProperties(
      BrowserDistribution* dist,
      const base::FilePath& target_exe,
      ShellUtil::ShortcutProperties* properties) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAppHostOperations);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_CHROME_APP_HOST_OPERATIONS_H_
