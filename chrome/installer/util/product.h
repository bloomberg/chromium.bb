// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"

class CommandLine;

namespace installer {

class ChannelInfo;
class MasterPreferences;
class Product;
class ProductOperations;

// Represents an installation of a specific product which has a one-to-one
// relation to a BrowserDistribution.  A product has registry settings, related
// installation/uninstallation actions and exactly one Package that represents
// the files on disk.  The Package may be shared with other Product instances,
// so only the last Product to be uninstalled should remove the package.
// Right now there are no classes that derive from Product, but in
// the future, as we move away from global functions and towards a data driven
// installation, each distribution could derive from this class and provide
// distribution specific functionality.
class Product {
 public:
  explicit Product(BrowserDistribution* distribution);

  ~Product();

  void InitializeFromPreferences(const MasterPreferences& prefs);

  void InitializeFromUninstallCommand(const CommandLine& uninstall_command);

  BrowserDistribution* distribution() const {
    return distribution_;
  }

  bool is_type(BrowserDistribution::Type type) const {
    return distribution_->GetType() == type;
  }

  bool is_chrome() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_BROWSER;
  }

  bool is_chrome_frame() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_FRAME;
  }

  bool is_chrome_app_host() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_APP_HOST;
  }

  bool is_chrome_binaries() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_BINARIES;
  }

  bool HasOption(const std::wstring& option) const {
    return options_.find(option) != options_.end();
  }

  // Returns true if the set of options is mutated by this operation.
  bool SetOption(const std::wstring& option, bool set) {
    if (set)
      return options_.insert(option).second;
    else
      return options_.erase(option) != 0;
  }

  // Returns the path(s) to the directory that holds the user data (primary
  // and, if applicable to |dist|, alternate).  This is always inside a user's
  // local application data folder (e.g., "AppData\Local or "Local
  // Settings\Application Data" in %USERPROFILE%). Note that these are the
  // defaults and do not take into account that they can be overriden with a
  // command line parameter.  |paths| may be empty on return, but is guaranteed
  // not to contain empty paths otherwise. If more than one path is returned,
  // they are guaranteed to be siblings.
  void GetUserDataPaths(std::vector<base::FilePath>* paths) const;

  // Launches Chrome without waiting for it to exit.
  bool LaunchChrome(const base::FilePath& application_path) const;

  // Launches Chrome with given command line, waits for Chrome indefinitely
  // (until it terminates), and gets the process exit code if available.
  // The function returns true as long as Chrome is successfully launched.
  // The status of Chrome at the return of the function is given by exit_code.
  // NOTE: The 'options' CommandLine object should only contain parameters.
  // The program part will be ignored.
  bool LaunchChromeAndWait(const base::FilePath& application_path,
                           const CommandLine& options,
                           int32* exit_code) const;

  // Sets the boolean MSI marker for this installation if set is true or clears
  // it otherwise. The MSI marker is stored in the registry under the
  // ClientState key.
  bool SetMsiMarker(bool system_install, bool set) const;

  // Returns true if setup should create an entry in the Add/Remove list
  // of installed applications.
  bool ShouldCreateUninstallEntry() const;

  // See ProductOperations::AddKeyFiles.
  void AddKeyFiles(std::vector<base::FilePath>* key_files) const;

  // See ProductOperations::AddComDllList.
  void AddComDllList(std::vector<base::FilePath>* com_dll_list) const;

  // See ProductOperations::AppendProductFlags.
  void AppendProductFlags(CommandLine* command_line) const;

  // See ProductOperations::AppendRenameFlags.
  void AppendRenameFlags(CommandLine* command_line) const;

  // See Productoperations::SetChannelFlags.
  bool SetChannelFlags(bool set, ChannelInfo* channel_info) const;

  // See ProductOperations::AddDefaultShortcutProperties.
  void AddDefaultShortcutProperties(
      const base::FilePath& target_exe,
      ShellUtil::ShortcutProperties* properties) const;

  void LaunchUserExperiment(const base::FilePath& setup_path,
                            InstallStatus status,
                            bool system_level) const;

 protected:
  enum CacheStateFlags {
    MSI_STATE = 0x01
  };

  BrowserDistribution* distribution_;
  scoped_ptr<ProductOperations> operations_;
  std::set<std::wstring> options_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Product);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_H_
