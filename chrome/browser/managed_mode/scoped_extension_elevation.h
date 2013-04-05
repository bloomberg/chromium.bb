// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_SCOPED_EXTENSION_ELEVATION_H_
#define CHROME_BROWSER_MANAGED_MODE_SCOPED_EXTENSION_ELEVATION_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

class ManagedUserService;

// Used to allow managed users to install extensions if they are currently in
// elevated state.
class ScopedExtensionElevation {
 public:
  explicit ScopedExtensionElevation(ManagedUserService* service);
  ~ScopedExtensionElevation();

  // Add elevation for the extension with the id |extension_id|.
  void AddExtension(const std::string& extension_id);

 private:
  ManagedUserService* service_;

  // A list of extensions which stay elevated until the destructor of this
  // class is called.
  std::vector<std::string> elevated_extensions_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionElevation);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_SCOPED_EXTENSION_ELEVATION_H_
