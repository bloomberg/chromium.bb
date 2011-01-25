// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"

class CommandLine;

namespace installer {

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

  // Returns the path to the directory that holds the user data.  This is always
  // inside "Users\<user>\Local Settings".  Note that this is the default user
  // data directory and does not take into account that it can be overriden with
  // a command line parameter.
  FilePath GetUserDataPath() const;

  // Launches Chrome without waiting for it to exit.
  bool LaunchChrome(const FilePath& application_path) const;

  // Launches Chrome with given command line, waits for Chrome indefinitely
  // (until it terminates), and gets the process exit code if available.
  // The function returns true as long as Chrome is successfully launched.
  // The status of Chrome at the return of the function is given by exit_code.
  // NOTE: The 'options' CommandLine object should only contain parameters.
  // The program part will be ignored.
  bool LaunchChromeAndWait(const FilePath& application_path,
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
  void AddKeyFiles(std::vector<FilePath>* key_files) const;

  // See ProductOperations::AddComDllList.
  void AddComDllList(std::vector<FilePath>* com_dll_list) const;

  // See ProductOperations::AppendProductFlags.
  void AppendProductFlags(CommandLine* command_line) const;

  // See Productoperations::SetChannelFlags.
  bool SetChannelFlags(bool set, ChannelInfo* channel_info) const;

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
