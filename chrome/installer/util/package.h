// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PACKAGE_H_
#define CHROME_INSTALLER_UTIL_PACKAGE_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"

class CommandLine;
class Version;

namespace installer {

enum InstallStatus;
class Product;
class PackageProperties;

typedef std::vector<scoped_refptr<const Product> > Products;

// Represents a physical installation.  An instance of this class is associated
// with one or more Product instances.  Product objects can share a Package but
// not vice versa.
class Package : public base::RefCounted<Package> {
 public:
  Package(bool multi_install, bool system_level, const FilePath& path,
          PackageProperties* properties);

  // Returns the path of the installation folder.
  const FilePath& path() const;

  const Products& products() const;

  PackageProperties* properties() const;

  bool multi_install() const;

  bool system_level() const;

  bool IsEqual(const FilePath& path) const;

  void AssociateProduct(const Product* product);

  // Get path to the installer under Chrome version folder
  // (for example <path>\Google\Chrome\Application\<Version>\Installer)
  FilePath GetInstallerDirectory(const Version& version) const;

  // Figure out the oldest currently installed version for this package
  // Returns NULL if none is found.  Caller is responsible for freeing
  // the returned Version object if valid.
  // The function DCHECKs if it finds that not all products in this
  // folder have the same current version.
  Version* GetCurrentVersion() const;

  // Tries to remove all previous version directories (after a new Chrome
  // update). If an instance of Chrome with older version is still running
  // on the system, its corresponding version directory will be left intact.
  // (The version directory is subject for removal again during next update.)
  //
  // latest_version: the latest version of Chrome installed.
  void RemoveOldVersionDirectories(const Version& latest_version) const;

  // Returns how many installed products depend on the binaries currently
  // in the installation path.
  // Note: The function counts only products that are installed as part of
  // a multi install installation and only products that have the same
  // system_level() value.
  size_t GetMultiInstallDependencyCount() const;

 protected:
  bool multi_install_;
  bool system_level_;
  FilePath path_;
  Products products_;
  PackageProperties* properties_;  // Weak reference.

 private:
  friend class base::RefCounted<Package>;
  ~Package();

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PACKAGE_H_
