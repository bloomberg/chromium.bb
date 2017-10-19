// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// LorgnetteManagerClient is used to communicate with the lorgnette
// document scanning daemon.
class CHROMEOS_EXPORT LorgnetteManagerClient : public DBusClient {
 public:
  // The property information for each scanner retured by ListScanners.
  typedef std::map<std::string, std::string> ScannerTableEntry;
  typedef std::map<std::string, ScannerTableEntry> ScannerTable;

  // Callback type for ListScanners().  Returns a map which contains
  // a ScannerTableEntry for each available scanner.
  typedef base::Callback<void(
      bool succeeded, const ScannerTable&)> ListScannersCallback;

  // Called once ScanImageToString() is complete. Takes two parameters:
  // - succeeded: was the scan completed successfully.
  // - image_data: the contents of the image.
  typedef base::Callback<void(
      bool succeeded,
      const std::string& image_data)> ScanImageToStringCallback;

  // Attributes provided to a scan request.
  struct ScanProperties {
    std::string mode;  // Can be "Color", "Gray", or "Lineart".
    int resolution_dpi = 0;
  };

  ~LorgnetteManagerClient() override;

  // Gets a list of scanners from the lorgnette manager.
  virtual void ListScanners(const ListScannersCallback& callback) = 0;

  // Request a scanned image and calls |callback| when completed with a string
  // pointing at the scanned image data.  Image data will be stored in the .png
  // format.
  virtual void ScanImageToString(std::string device_name,
                                 const ScanProperties& properties,
                                 const ScanImageToStringCallback& callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static LorgnetteManagerClient* Create();

 protected:
  // Create() should be used instead.
  LorgnetteManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(LorgnetteManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_
