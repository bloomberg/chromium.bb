// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_

#include <string>
#include "base/basictypes.h"

namespace chromeos {

// This class handles the loading of the ChromeOS shared library.
class CrosLibrary {
 public:
  // Ensures that the library is loaded, loading it if needed. If the library
  // could not be loaded, returns false.
  static bool EnsureLoaded();

  // Returns an unlocalized string describing the last load error (if any).
  static const std::string& load_error_string() {
    return load_error_string_;
  }

 private:
  CrosLibrary() {}
  ~CrosLibrary() {}

  // True if libcros was successfully loaded.
  static bool loaded_;
  // True if the last load attempt had an error.
  static bool load_error_;
  // Contains the error string from the last load attempt.
  static std::string load_error_string_;

  DISALLOW_COPY_AND_ASSIGN(CrosLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
