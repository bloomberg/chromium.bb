// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_controller.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace chromeos {
namespace imageburner {

namespace {

// 3.9GB. It is less than 4GB because true device size ussually varies a little.
const uint64 kMinDeviceSize = static_cast<uint64>(3.9 * 1000 * 1000 * 1000);

class BurnControllerImpl
    : public BurnController,
      public StateMachine::Observer,
      public BurnManager::Observer {
 public:
  explicit BurnControllerImpl(BurnController::Delegate* delegate)
      : burn_manager_(NULL),
        state_machine_(NULL),
        working_(false),
        delegate_(delegate) {
    burn_manager_ = BurnManager::GetInstance();
    burn_manager_->AddObserver(this);
    state_machine_ = burn_manager_->state_machine();
    state_machine_->AddObserver(this);
  }

  virtual ~BurnControllerImpl() {
    state_machine_->RemoveObserver(this);
    burn_manager_->RemoveObserver(this);
  }

  // BurnManager::Observer override.
  virtual void OnDeviceAdded(
      const disks::DiskMountManager::Disk& disk) OVERRIDE {
    delegate_->OnDeviceAdded(disk);
  }

  // BurnManager::Observer override.
  virtual void OnDeviceRemoved(
      const disks::DiskMountManager::Disk& disk) OVERRIDE {
    delegate_->OnDeviceRemoved(disk);
  }

  // BurnManager::Observer override.
  virtual void OnNetworkDetected() OVERRIDE {
    delegate_->OnNetworkDetected();
  }

  // BurnManager::Observer override.
  virtual void OnImageDirCreated(bool success) OVERRIDE {
    if (!success) {
      // Failed to create the directory. Finish the burning process
      // with failure state.
      DownloadCompleted(false);
      return;
    }

    burn_manager_->FetchConfigFile();
  }

  // BurnManager::Observer override.
  virtual void OnConfigFileFetched(bool success) OVERRIDE {
    if (!success) {
      DownloadCompleted(false);
      return;
    }

    if (state_machine_->download_finished()) {
      BurnImage();
      return;
    }

    if (!state_machine_->download_started()) {
      burn_manager_->FetchImage();
      state_machine_->OnDownloadStarted();
    }
  }

  // BurnManager::Observer override.
  virtual void OnImageFileFetchDownloadProgressUpdated(
      int64 received_bytes,
      int64 total_bytes,
      const base::TimeDelta& estimated_remaining_time) OVERRIDE {
    if (state_machine_->state() == StateMachine::DOWNLOADING) {
      delegate_->OnProgressWithRemainingTime(DOWNLOADING,
                                             received_bytes,
                                             total_bytes,
                                             estimated_remaining_time);
    }
  }

  // BurnManager::Observer override.
  virtual void OnImageFileFetched(bool success) OVERRIDE {
    DownloadCompleted(success);
  }

  // BurnManager::Observer override.
  virtual void OnBurnProgressUpdated(BurnEvent event,
                                     const ImageBurnStatus& status) OVERRIDE {
    switch (event) {
      case(BURN_SUCCESS):
        FinalizeBurn();
        break;
      case(BURN_FAIL):
        burn_manager_->OnError(IDS_IMAGEBURN_BURN_ERROR);
        break;
      case(BURN_UPDATE):
        delegate_->OnProgress(BURNING, status.amount_burnt, status.total_size);
        break;
      case(UNZIP_STARTED):
        delegate_->OnProgress(UNZIPPING, 0, 0);
        break;
      case(UNZIP_FAIL):
        burn_manager_->OnError(IDS_IMAGEBURN_EXTRACTING_ERROR);
        break;
      case(UNZIP_COMPLETE):
        // We ignore this.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // StateMachine::Observer interface.
  virtual void OnBurnStateChanged(StateMachine::State state) OVERRIDE {
    if (state != StateMachine::INITIAL && !working_) {
      // User has started burn process, so let's start observing.
      StartBurnImage(base::FilePath(), base::FilePath());
    }
  }

  virtual void OnError(int error_message_id) OVERRIDE {
    delegate_->OnFail(error_message_id);
    working_ = false;
  }

  // BurnController override.
  virtual void Init() OVERRIDE {
    if (state_machine_->state() == StateMachine::BURNING) {
      // There is nothing else left to do but observe burn progress.
      BurnImage();
    } else if (state_machine_->state() != StateMachine::INITIAL) {
      // User has started burn process, so let's start observing.
      StartBurnImage(base::FilePath(), base::FilePath());
    }
  }

  // BurnController override.
  virtual std::vector<disks::DiskMountManager::Disk> GetBurnableDevices()
      OVERRIDE {
    // Now this is just a proxy to the BurnManager.
    // TODO(hidehiko): Remove this method.
    return burn_manager_->GetBurnableDevices();
  }

  // BurnController override.
  virtual void CancelBurnImage() OVERRIDE {
    burn_manager_->Cancel();
  }

  // BurnController override.
  // May be called with empty values if there is a handler that has started
  // burning, and thus set the target paths.
  virtual void StartBurnImage(const base::FilePath& target_device_path,
                              const base::FilePath& target_file_path) OVERRIDE {
    if (!target_device_path.empty() && !target_file_path.empty() &&
        state_machine_->new_burn_posible()) {
      if (!burn_manager_->IsNetworkConnected()) {
        delegate_->OnNoNetwork();
        return;
      }
      burn_manager_->set_target_device_path(target_device_path);
      burn_manager_->set_target_file_path(target_file_path);
      uint64 device_size = GetDeviceSize(
          burn_manager_->target_device_path().value());
      if (device_size < kMinDeviceSize) {
        delegate_->OnDeviceTooSmall(device_size);
        return;
      }
    }
    if (working_)
      return;
    working_ = true;
    // Send progress signal now so ui doesn't hang in intial state until we get
    // config file
    delegate_->OnProgress(DOWNLOADING, 0, 0);
    if (burn_manager_->GetImageDir().empty()) {
      burn_manager_->CreateImageDir();
    } else {
      OnImageDirCreated(true);
    }
  }

 private:
  void DownloadCompleted(bool success) {
    if (success) {
      state_machine_->OnDownloadFinished();
      BurnImage();
    } else {
      burn_manager_->OnError(IDS_IMAGEBURN_DOWNLOAD_ERROR);
    }
  }

  void BurnImage() {
    if (state_machine_->state() == StateMachine::BURNING)
      return;
    state_machine_->OnBurnStarted();
    burn_manager_->DoBurn();
  }

  void FinalizeBurn() {
    state_machine_->OnSuccess();
    burn_manager_->ResetTargetPaths();
    delegate_->OnSuccess();
    working_ = false;
  }

  int64 GetDeviceSize(const std::string& device_path) {
    disks::DiskMountManager* disk_mount_manager =
        disks::DiskMountManager::GetInstance();
    const disks::DiskMountManager::Disk* disk =
        disk_mount_manager->FindDiskBySourcePath(device_path);
    return disk ? disk->total_size_in_bytes() : 0;
  }

  BurnManager* burn_manager_;
  StateMachine* state_machine_;
  bool working_;
  BurnController::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BurnControllerImpl);
};

}  // namespace

// static
BurnController* BurnController::CreateBurnController(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return new BurnControllerImpl(delegate);
}

}  // namespace imageburner
}  // namespace chromeos
