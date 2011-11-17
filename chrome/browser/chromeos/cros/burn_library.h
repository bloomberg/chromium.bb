// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "third_party/cros/chromeos_imageburn.h"

struct ImageBurnStatus {
  ImageBurnStatus() : target_path(),
                      amount_burnt(0),
                      total_size(0),
                      error() {
  }

  explicit ImageBurnStatus(const chromeos::BurnStatus& status)
      : amount_burnt(status.amount_burnt), total_size(status.total_size) {
    if (status.target_path)
      target_path = status.target_path;
    if (status.error)
      error = status.error;
  }

  ImageBurnStatus(const char* path, int64 burnt, int64 total, const char* err)
      : total_size(total) {
    if (path)
      target_path = path;
    if (err)
      error = err;
  }

  std::string target_path;
  int64 amount_burnt;
  int64 total_size;
  std::string error;
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
    virtual void BurnProgressUpdated(BurnLibrary* object, BurnEvent evt,
                                 const ImageBurnStatus& status) = 0;
  };

  virtual ~BurnLibrary() {}

  virtual void Init() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Example values:
  // DoBurn(image.bin.zip, image.bin, /dev/sdb, /sys/devices/pci..../block.sdb).
  virtual void DoBurn(const FilePath& source_path,
                      const std::string& image_name,
                      const FilePath& target_file_path,
                      const FilePath& target_device_path) = 0;
  virtual void CancelBurnImage() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static BurnLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
