// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_H_

#include "base/basictypes.h"

namespace chromeos {

// This class handles the loading of the ChromeOS shared library.
class CrosLibrary {
 public:
  // Ensures that the library is loaded, loading it if needed. If the library
  // could not be loaded, returns false.
  static bool EnsureLoaded();

 private:
  CrosLibrary() {}
  ~CrosLibrary() {}

  static bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(CrosLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_H_
