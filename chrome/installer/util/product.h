// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_H_
#pragma once

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/package.h"

class CommandLine;

namespace installer {
class MasterPreferences;
}

namespace installer {

class Product;
class Package;
class PackageProperties;

typedef std::vector<scoped_refptr<Package> > Packages;
typedef std::vector<scoped_refptr<const Product> > Products;

const Product* FindProduct(const Products& products,
                           BrowserDistribution::Type type);

// Represents an installation of a specific product which has a one-to-one
// relation to a BrowserDistribution.  A product has registry settings, related
// installation/uninstallation actions and exactly one Package that represents
// the files on disk.  The Package may be shared with other Product instances,
// so only the last Product to be uninstalled should remove the package.
// Right now there are no classes that derive from Product, but in
// the future, as we move away from global functions and towards a data driven
// installation, each distribution could derive from this class and provide
// distribution specific functionality.
class Product : public base::RefCounted<Product> {
 public:
  Product(BrowserDistribution* distribution, Package* installation_package);

  BrowserDistribution* distribution() const {
    return distribution_;
  }

  // Returns the install package object for the installation of this product.
  // If the product is installed at system level,the function returns a system
  // wide location (ProgramFiles\Google). Otherwise it returns a package in a
  // user specific location (Users\<user>\Local Settings...)
  const Package& package() const;

  // Convenience getter for package().system_level().
  bool system_level() const {
    return package().system_level();
  }

  bool is_chrome() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_BROWSER;
  }

  bool is_chrome_frame() const {
    return distribution_->GetType() == BrowserDistribution::CHROME_FRAME;
  }

  // Returns the path to the directory that holds the user data.  This is always
  // inside "Users\<user>\Local Settings".  Note that this is the default user
  // data directory and does not take into account that it can be overriden with
  // a command line parameter.
  FilePath GetUserDataPath() const;

  // Launches Chrome without waiting for it to exit.
  bool LaunchChrome() const;

  // Launches Chrome with given command line, waits for Chrome indefinitely
  // (until it terminates), and gets the process exit code if available.
  // The function returns true as long as Chrome is successfully launched.
  // The status of Chrome at the return of the function is given by exit_code.
  // NOTE: The 'options' CommandLine object should only contain parameters.
  // The program part will be ignored.
  bool LaunchChromeAndWait(const CommandLine& options, int32* exit_code) const;

  // Returns true if this setup process is running as an install managed by an
  // MSI wrapper. There are three things that are checked:
  // 1) the presence of --msi on the command line
  // 2) the presence of "msi": true in the master preferences file
  // 3) the presence of a DWORD value in the ClientState key called msi with
  //    value 1
  bool IsMsi() const;

  // Sets the boolean MSI marker for this installation if set is true or clears
  // it otherwise. The MSI marker is stored in the registry under the
  // ClientState key.
  bool SetMsiMarker(bool set) const;

  // Returns true if setup should create an entry in the Add/Remove list
  // of installed applications.
  bool ShouldCreateUninstallEntry() const;

 protected:
  enum CacheStateFlags {
    MSI_STATE = 0x01
  };

  BrowserDistribution* distribution_;
  scoped_refptr<Package> package_;
  mutable bool msi_;
  mutable uint8 cache_state_;

 private:
  friend class base::RefCounted<Product>;
  ~Product() {
  }
  DISALLOW_COPY_AND_ASSIGN(Product);
};

// A collection of Product objects and related physical installation
// packages.  Each Product is associated with a single installation
// package object, and each package object is associated with one or more
// Product objects.
class ProductPackageMapping {
 public:
  explicit ProductPackageMapping(bool multi_install, bool system_level);

  bool multi_install() const {
    return multi_install_;
  }

  bool system_level() const {
    return system_level_;
  }

  const Packages& packages() const;

  const Products& products() const;

  bool AddDistribution(BrowserDistribution::Type type,
                       const installer::MasterPreferences& prefs);
  bool AddDistribution(BrowserDistribution* distribution);

 protected:
  bool multi_install_;
  bool system_level_;
  Packages packages_;
  Products products_;
  scoped_ptr<PackageProperties> package_properties_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProductPackageMapping);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_H_
