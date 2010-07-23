// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_LIBRARY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_LIBRARY_LOADER_H_

#include <string>

#include "chrome/browser/chromeos/cros/cros_library_loader.h"

namespace chromeos {

// Fake for libcros library loader.
class  FakeLibraryLoader : public LibraryLoader {
 public:
  FakeLibraryLoader() {}
  virtual ~FakeLibraryLoader() {}
  virtual bool Load(std::string* load_error_string) {
    return true;
  }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_LIBRARY_LOADER_H_
