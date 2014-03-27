// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVER_H_

#include <string>

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystem;

// Observes file_system_provider::Service for mounting and unmounting events.
class Observer {
 public:
  // Called when a provided |file_system| is registered successfully.
  virtual void OnProvidedFileSystemRegistered(
      const ProvidedFileSystem& file_system) = 0;

  // Called when a provided |file_system| is unregistered successfully.
  virtual void OnProvidedFileSystemUnregistered(
      const ProvidedFileSystem& file_system) = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVER_H_
