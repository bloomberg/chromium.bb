// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace base {
class FilePath;
class TimeDelta;
}

namespace content {
class WebContents;
}

namespace chromeos {
namespace imageburner {

// A class to control recovery media creating process.
class BurnController {
 public:
  class Delegate {
   public:
    // Called when recovery image is successfully burnt.
    virtual void OnSuccess() = 0;
    // Called when something goes wrong.
    virtual void OnFail(int error_message_id) = 0;
    // Called when a burnable device is added.
    virtual void OnDeviceAdded(const disks::DiskMountManager::Disk& disk) = 0;
    // Called when a burnable device is removed.
    virtual void OnDeviceRemoved(const disks::DiskMountManager::Disk& disk) = 0;
    // Called when device is too small.
    virtual void OnDeviceTooSmall(int64 device_size) = 0;
    // Called when some progress is made.
    virtual void OnProgress(ProgressType progress_type,
                            int64 amount_finished,
                            int64 amount_total) = 0;
    // Called when some progress is made and remaining time estimation is
    // available.
    virtual void OnProgressWithRemainingTime(
        ProgressType progress_type,
        int64 amount_finished,
        int64 amount_total,
        const base::TimeDelta& time_remaining) = 0;
    // Called when network is connected.
    virtual void OnNetworkDetected() = 0;
    // Called when an error occurs because there is no network connection.
    virtual void OnNoNetwork() = 0;
  };

  virtual ~BurnController() {}

  // Initializes the instance.
  virtual void Init() = 0;
  // Returns devices on which we can burn recovery image.
  virtual std::vector<disks::DiskMountManager::Disk> GetBurnableDevices() = 0;
  // Starts burning process.
  virtual void StartBurnImage(const base::FilePath& target_device_path,
                              const base::FilePath& target_file_path) = 0;
  // Cancels burning process.
  virtual void CancelBurnImage() = 0;
  // Creates a new instance of BurnController.
  static BurnController* CreateBurnController(
      content::WebContents* web_contents, Delegate* delegate);

 protected:
  BurnController() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BurnController);
};

}  // namespace imageburner
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_CONTROLLER_H_
