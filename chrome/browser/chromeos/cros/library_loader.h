// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_LOADER_H_
#pragma once

#include <string>

namespace chromeos {

// This interface loads the libcros library.
class LibraryLoader {
 public:
  virtual ~LibraryLoader() {}
  virtual bool Load(std::string* load_error_string) = 0;

  // Factory function, creates a new instance and returns ownership.
  static LibraryLoader* GetImpl();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LIBRARY_LOADER_H_
