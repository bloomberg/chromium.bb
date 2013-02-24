// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_

#include <string>

#include "base/files/file_path.h"

struct ImageBurnStatus {
  ImageBurnStatus() : amount_burnt(0), total_size(0) {
  }

  ImageBurnStatus(int64 burnt, int64 total)
      : amount_burnt(burnt), total_size(total) {
  }

  int64 amount_burnt;
  int64 total_size;
};

namespace chromeos {

enum BurnEvent {
  UNZIP_STARTED,
  UNZIP_COMPLETE,
  UNZIP_FAIL,
  BURN_UPDATE,
  BURN_SUCCESS,
  BURN_FAIL,
  UNKNOWN
};

class BurnLibrary {
 public:
  class Observer {
   public:
    virtual void BurnProgressUpdated(BurnLibrary* object,
                                     BurnEvent evt,
                                     const ImageBurnStatus& status) = 0;
  };

  virtual ~BurnLibrary() {}

  virtual void Init() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Example values:
  // DoBurn(image.bin.zip, image.bin, /dev/sdb, /sys/devices/pci..../block.sdb).
  virtual void DoBurn(const base::FilePath& source_path,
                      const std::string& image_name,
                      const base::FilePath& target_file_path,
                      const base::FilePath& target_device_path) = 0;
  virtual void CancelBurnImage() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static BurnLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
