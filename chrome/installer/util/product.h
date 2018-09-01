// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_H_

#include "base/macros.h"

class BrowserDistribution;

namespace installer {

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

  BrowserDistribution* distribution() const {
    return distribution_;
  }

 protected:
  BrowserDistribution* const distribution_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Product);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_H_
