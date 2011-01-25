// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_OPERATIONS_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_OPERATIONS_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"

class CommandLine;

namespace installer {

class ChannelInfo;
class MasterPreferences;

// An interface to product-specific operations that depend on product
// configuration.  Implementations are expected to be stateless.  Configuration
// can be read from a MasterPreferences instance or from a product's uninstall
// command.
class ProductOperations {
 public:
  virtual ~ProductOperations() {}

  // Reads product-specific options from |prefs|, adding them to |options|.
  virtual void ReadOptions(const MasterPreferences& prefs,
                           std::set<std::wstring>* options) const = 0;

  // Reads product-specific options from |command|, adding them to |options|.
  virtual void ReadOptions(const CommandLine& command,
                           std::set<std::wstring>* options) const = 0;

  // A key-file is a file such as a DLL on Windows that is expected to be in use
  // when the product is being used.  For example "chrome.dll" for Chrome.
  // Before attempting to delete an installation directory during an
  // uninstallation, the uninstaller will check if any one of a potential set of
  // key files is in use and if they are, abort the delete operation.  Only if
  // none of the key files are in use, can the folder be deleted.  Note that
  // this function does not return a full path to the key file(s), only (a) file
  // name(s).
  virtual void AddKeyFiles(const std::set<std::wstring>& options,
                           std::vector<FilePath>* key_files) const = 0;

  // Adds to |com_dll_list| the list of COM DLLs that are to be registered
  // and/or unregistered. The list may be empty.
  virtual void AddComDllList(const std::set<std::wstring>& options,
                             std::vector<FilePath>* com_dll_list) const = 0;

  // Given a command line, appends the set of uninstall flags the uninstaller
  // for this product will require.
  virtual void AppendProductFlags(const std::set<std::wstring>& options,
                                  CommandLine* uninstall_command) const = 0;

  // Adds or removes product-specific flags in |channel_info|.  Returns true if
  // |channel_info| is modified.
  virtual bool SetChannelFlags(const std::set<std::wstring>& options,
                               bool set,
                               ChannelInfo* channel_info) const = 0;

  // Returns true if setup should create an entry in the Add/Remove list
  // of installed applications for this product.  This does not test for use of
  // MSI; see InstallerState::is_msi.
  virtual bool ShouldCreateUninstallEntry(
      const std::set<std::wstring>& options) const = 0;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_OPERATIONS_H_
