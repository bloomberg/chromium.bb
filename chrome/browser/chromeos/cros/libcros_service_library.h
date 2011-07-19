// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LIBCROS_SERVICE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LIBCROS_SERVICE_LIBRARY_H_
#pragma once

namespace net{
class ProxyService;
};

namespace chromeos {

class LibCrosServiceLibrary {
 public:
  virtual ~LibCrosServiceLibrary() {}

  // Starts dbus service for LibCrosService.
  virtual void StartService() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static LibCrosServiceLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LIBCROS_SERVICE_LIBRARY_H_
