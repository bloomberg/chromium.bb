// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_controller.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "googleurl/src/gurl.h"

namespace chromeos {
namespace imageburner {

namespace {

// Name for hwid in machine statistics.
const char kHwidStatistic[] = "hardware_class";

const char kImageZipFileName[] = "chromeos_image.bin.zip";

// 3.9GB. It is less than 4GB because true device size ussually varies a little.
const uint64 kMinDeviceSize = static_cast<uint64>(3.9 * 1000 * 1000 * 1000);

// Returns true when |disk| is a device on which we can burn recovery image.
bool IsBurnableDevice(const disks::DiskMountManager::Disk& disk) {
  return disk.is_parent() && !disk.on_boot_device() &&
         (disk.device_type() == DEVICE_TYPE_USB ||
          disk.device_type() == DEVICE_TYPE_SD);
}

class BurnControllerImpl
    : public BurnController,
      public disks::DiskMountManager::Observer,
      public BurnLibrary::Observer,
      public NetworkLibrary::NetworkManagerObserver,
      public content::DownloadItem::Observer,
      public content::DownloadManager::Observer,
      public Downloader::Listener,
      public StateMachine::Observer,
      public BurnManager::Delegate {
 public:
  explicit BurnControllerImpl(content::WebContents* contents,
                              BurnController::Delegate* delegate)
      : web_contents_(contents),
        download_manager_(NULL),
        active_download_item_(NULL),
        burn_manager_(NULL),
        state_machine_(NULL),
        observing_burn_lib_(false),
        working_(false),
        delegate_(delegate) {
    disks::DiskMountManager::GetInstance()->AddObserver(this);
    CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
    burn_manager_ = BurnManager::GetInstance();
    state_machine_ = burn_manager_->state_machine();
    state_machine_->AddObserver(this);
  }

  virtual ~BurnControllerImpl() {
    disks::DiskMountManager::GetInstance()->RemoveObserver(this);
    CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
    CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
    CleanupDownloadObjects();
    if (state_machine_)
      state_machine_->RemoveObserver(this);
  }

  // disks::DiskMountManager::Observer interface.
  virtual void DiskChanged(disks::DiskMountManagerEventType event,
                           const disks::DiskMountManager::Disk* disk)
      OVERRIDE {
    if (!IsBurnableDevice(*disk))
      return;
    if (event == disks::MOUNT_DISK_ADDED) {
      delegate_->OnDeviceAdded(*disk);
    } else if (event == disks::MOUNT_DISK_REMOVED) {
      delegate_->OnDeviceRemoved(*disk);
      if (burn_manager_->target_device_path().value() == disk->device_path())
        ProcessError(IDS_IMAGEBURN_DEVICE_NOT_FOUND_ERROR);
    }
  }

  virtual void DeviceChanged(disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE {
  }

  virtual void MountCompleted(
      disks::DiskMountManager::MountEvent event_type,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE {
  }

  // BurnLibrary::Observer interface.
  virtual void BurnProgressUpdated(BurnLibrary* object,
                                   BurnEvent evt,
                                   const ImageBurnStatus& status) OVERRIDE {
    switch (evt) {
      case(BURN_SUCCESS):
        FinalizeBurn();
        break;
      case(BURN_FAIL):
        ProcessError(IDS_IMAGEBURN_BURN_ERROR);
        break;
      case(BURN_UPDATE):
        delegate_->OnProgress(BURNING, status.amount_burnt, status.total_size);
        break;
      case(UNZIP_STARTED):
        delegate_->OnProgress(UNZIPPING, 0, 0);
        break;
      case(UNZIP_FAIL):
        ProcessError(IDS_IMAGEBURN_EXTRACTING_ERROR);
        break;
      case(UNZIP_COMPLETE):
        // We ignore this.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // NetworkLibrary::NetworkManagerObserver interface.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE {
    if (state_machine_->state() == StateMachine::INITIAL && CheckNetwork())
      delegate_->OnNetworkDetected();

    if (state_machine_->state() == StateMachine::DOWNLOADING &&
        !CheckNetwork())
      ProcessError(IDS_IMAGEBURN_NETWORK_ERROR);
  }

  // content::DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE {
    if (download->IsCancelled()) {
      DownloadCompleted(false);
      DCHECK(!active_download_item_);
    } else if (download->IsComplete()) {
      DownloadCompleted(true);
      DCHECK(!active_download_item_);
    } else if (download->IsPartialDownload() &&
               state_machine_->state() == StateMachine::DOWNLOADING) {
      base::TimeDelta remaining_time;
      download->TimeRemaining(&remaining_time);
      delegate_->OnProgressWithRemainingTime(
          DOWNLOADING, download->GetReceivedBytes(), download->GetTotalBytes(),
          remaining_time);
    }
  }

  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {
    if (download->GetSafetyState() == content::DownloadItem::DANGEROUS)
      download->DangerousDownloadValidated();
  }

  // content::DownloadManager::Observer interface.
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE {
    DCHECK_EQ(download_manager_, manager);
    // Find our item and observe it.
    std::vector<content::DownloadItem*> downloads;
    download_manager_->GetTemporaryDownloads(
        burn_manager_->GetImageDir(), &downloads);
    if (active_download_item_)
      return;
    for (std::vector<content::DownloadItem*>::const_iterator it =
             downloads.begin(); it != downloads.end(); ++it) {
      if ((*it)->GetOriginalUrl() == image_download_url_) {
        (*it)->AddObserver(this);
        active_download_item_ = *it;
        break;
      }
    }
  }

  // Downloader::Listener interface.
  virtual void OnBurnDownloadStarted(bool success) OVERRIDE {
    if (success)
      state_machine_->OnDownloadStarted();
    else
      DownloadCompleted(false);
  }

  // StateMachine::Observer interface.
  virtual void OnBurnStateChanged(StateMachine::State state) OVERRIDE {
    if (state == StateMachine::CANCELLED) {
      ProcessError(IDS_IMAGEBURN_USER_ERROR);
    } else if (state != StateMachine::INITIAL && !working_) {
      // User has started burn process, so let's start observing.
      StartBurnImage(FilePath(), FilePath());
    }
  }

  virtual void OnError(int error_message_id) OVERRIDE {
    delegate_->OnFail(error_message_id);
    working_ = false;
  }

  // Part of BurnManager::Delegate interface.
  virtual void OnImageDirCreated(bool success) OVERRIDE {
    if (success) {
      zip_image_file_path_ =
          burn_manager_->GetImageDir().Append(kImageZipFileName);
      burn_manager_->FetchConfigFile(this);
    } else {
      DownloadCompleted(success);
    }
  }

  // Part of BurnManager::Delegate interface.
  virtual void OnConfigFileFetched(const ConfigFile& config_file, bool success)
      OVERRIDE {
    if (!success || !ExtractInfoFromConfigFile(config_file)) {
      DownloadCompleted(false);
      return;
    }

    if (state_machine_->download_finished()) {
      BurnImage();
      return;
    }

    if (!download_manager_) {
      download_manager_ =
          web_contents_->GetBrowserContext()->GetDownloadManager();
      download_manager_->AddObserver(this);
    }
    if (!state_machine_->download_started()) {
      burn_manager_->downloader()->AddListener(this, image_download_url_);
      if (!state_machine_->image_download_requested()) {
        state_machine_->OnImageDownloadRequested();
        burn_manager_->downloader()->DownloadFile(image_download_url_,
                                                  zip_image_file_path_,
                                                  web_contents_);
      }
    }
  }

  // BurnController override.
  virtual void Init() OVERRIDE {
    if (state_machine_->state() == StateMachine::BURNING) {
      // There is nothing else left to do but observe burn progress.
      BurnImage();
    } else if (state_machine_->state() != StateMachine::INITIAL) {
      // User has started burn process, so let's start observing.
      StartBurnImage(FilePath(), FilePath());
    }
  }

  // BurnController override.
  virtual std::vector<disks::DiskMountManager::Disk> GetBurnableDevices()
      OVERRIDE {
    const disks::DiskMountManager::DiskMap& disks =
        disks::DiskMountManager::GetInstance()->disks();
    std::vector<disks::DiskMountManager::Disk> result;
    for (disks::DiskMountManager::DiskMap::const_iterator iter = disks.begin();
         iter != disks.end();
         ++iter) {
      const disks::DiskMountManager::Disk& disk = *iter->second;
      if (IsBurnableDevice(disk))
        result.push_back(disk);
    }
    return result;
  }

  // BurnController override.
  virtual void CancelBurnImage() OVERRIDE {
    state_machine_->OnCancelation();
  }

  // BurnController override.
  // May be called with empty values if there is a handler that has started
  // burning, and thus set the target paths.
  virtual void StartBurnImage(const FilePath& target_device_path,
                              const FilePath& target_file_path) OVERRIDE {
    if (!target_device_path.empty() && !target_file_path.empty() &&
        state_machine_->new_burn_posible()) {
      if (!CheckNetwork()) {
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
      burn_manager_->CreateImageDir(this);
    } else {
      OnImageDirCreated(true);
    }
  }

 private:
  void DownloadCompleted(bool success) {
    CleanupDownloadObjects();
    if (success) {
      state_machine_->OnDownloadFinished();
      BurnImage();
    } else {
      ProcessError(IDS_IMAGEBURN_DOWNLOAD_ERROR);
    }
  }

  void BurnImage() {
    if (!observing_burn_lib_) {
      CrosLibrary::Get()->GetBurnLibrary()->AddObserver(this);
      observing_burn_lib_ = true;
    }
    if (state_machine_->state() == StateMachine::BURNING)
      return;
    state_machine_->OnBurnStarted();

    CrosLibrary::Get()->GetBurnLibrary()->DoBurn(
        zip_image_file_path_,
        image_file_name_, burn_manager_->target_file_path(),
        burn_manager_->target_device_path());
  }

  void FinalizeBurn() {
    state_machine_->OnSuccess();
    burn_manager_->ResetTargetPaths();
    CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
    observing_burn_lib_ = false;
    delegate_->OnSuccess();
    working_ = false;
  }

  // Error is ussually detected by all existing Burn handlers, but only first
  // one that calls ProcessError should actually process it.
  void ProcessError(int message_id) {
    // If we are in intial state, error has already been dispached.
    if (state_machine_->state() == StateMachine::INITIAL) {
      // We don't need burn library since we are not the ones doing the cleanup.
      if (observing_burn_lib_) {
        CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
        observing_burn_lib_ = false;
      }
      return;
    }

    // Remember burner state, since it will be reset after OnError call.
    StateMachine::State state = state_machine_->state();

    // Dispach error. All hadlers' OnError event will be called before returning
    // from this. This includes us, too.
    state_machine_->OnError(message_id);

    // Do cleanup.
    if (state  == StateMachine::DOWNLOADING) {
      if (active_download_item_) {
        // This will trigger Download canceled event. As a response to that
        // event, handlers will remove themselves as observers from download
        // manager and item.
        // We don't want to process Download Cancelled signal.
        active_download_item_->RemoveObserver(this);
        if (active_download_item_->IsPartialDownload())
          active_download_item_->Cancel(true);
        active_download_item_->Delete(
            content::DownloadItem::DELETE_DUE_TO_USER_DISCARD);
        active_download_item_ = NULL;
        CleanupDownloadObjects();
      }
    } else if (state == StateMachine::BURNING) {
      DCHECK(observing_burn_lib_);
      // Burn library doesn't send cancelled signal upon CancelBurnImage
      // invokation.
      CrosLibrary::Get()->GetBurnLibrary()->CancelBurnImage();
      CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
      observing_burn_lib_ = false;
    }
    burn_manager_->ResetTargetPaths();
  }

  bool ExtractInfoFromConfigFile(const ConfigFile& config_file) {
    std::string hwid;
    if (!system::StatisticsProvider::GetInstance()->
        GetMachineStatistic(kHwidStatistic, &hwid))
      return false;

    image_file_name_ = config_file.GetProperty(kFileName, hwid);
    if (image_file_name_.empty())
      return false;

    image_download_url_ = GURL(config_file.GetProperty(kUrl, hwid));
    if (image_download_url_.is_empty()) {
      image_file_name_.clear();
      return false;
    }

    return true;
  }

  void CleanupDownloadObjects() {
    if (active_download_item_) {
      active_download_item_->RemoveObserver(this);
      active_download_item_ = NULL;
    }
    if (download_manager_) {
      download_manager_->RemoveObserver(this);
      download_manager_ = NULL;
    }
  }

  int64 GetDeviceSize(const std::string& device_path) {
    disks::DiskMountManager* disk_mount_manager =
        disks::DiskMountManager::GetInstance();
    const disks::DiskMountManager::DiskMap& disks = disk_mount_manager->disks();
    return disks.find(device_path)->second->total_size_in_bytes();
  }

  bool CheckNetwork() {
    return CrosLibrary::Get()->GetNetworkLibrary()->Connected();
  }

  FilePath zip_image_file_path_;
  GURL image_download_url_;
  std::string image_file_name_;
  content::WebContents* web_contents_;
  content::DownloadManager* download_manager_;
  content::DownloadItem*  active_download_item_;
  BurnManager* burn_manager_;
  StateMachine* state_machine_;
  bool observing_burn_lib_;
  bool working_;
  BurnController::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BurnControllerImpl);
};

}  // namespace

// static
BurnController* BurnController::CreateBurnController(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return new BurnControllerImpl(web_contents, delegate);
}

}  // namespace imageburner
}  // namespace chromeos
