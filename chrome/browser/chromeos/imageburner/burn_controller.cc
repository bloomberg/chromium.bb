// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_controller.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "chromeos/network/network_state_handler.h"
#include "url/gurl.h"

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
  virtual void OnSuccess() OVERRIDE {
    delegate_->OnSuccess();
    // TODO(hidehiko): Remove |working_| flag.
    working_ = false;
  }

  // BurnManager::Observer override.
  virtual void OnProgressWithRemainingTime(
      ProgressType progress_type,
      int64 received_bytes,
      int64 total_bytes,
      const base::TimeDelta& estimated_remaining_time) OVERRIDE {
    delegate_->OnProgressWithRemainingTime(
        progress_type, received_bytes, total_bytes, estimated_remaining_time);
  }

  // BurnManager::Observer override.
  virtual void OnProgress(ProgressType progress_type,
                          int64 received_bytes,
                          int64 total_bytes) OVERRIDE {
    delegate_->OnProgress(progress_type, received_bytes, total_bytes);
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
      burn_manager_->DoBurn();
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
      if (!NetworkHandler::Get()->network_state_handler()->DefaultNetwork()) {
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
      burn_manager_->FetchConfigFile();
    }
  }

 private:
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
