// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace chromeos {
namespace imageburner {

namespace {

// Name for hwid in machine statistics.
const char kHwidStatistic[] = "hardware_class";

const char kConfigFileUrl[] =
    "https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
const char kTempImageFolderName[] = "chromeos_image";

const char kImageZipFileName[] = "chromeos_image.bin.zip";

const int64 kBytesImageDownloadProgressReportInterval = 10240;

BurnManager* g_burn_manager = NULL;

// Cretes a directory and calls |callback| with the result on UI thread.
void CreateDirectory(const base::FilePath& path,
                     base::Callback<void(bool success)> callback) {
  const bool success = file_util::CreateDirectory(path);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

}  // namespace

const char kName[] = "name";
const char kHwid[] = "hwid";
const char kFileName[] = "file";
const char kUrl[] = "url";

////////////////////////////////////////////////////////////////////////////////
//
// ConfigFile
//
////////////////////////////////////////////////////////////////////////////////
ConfigFile::ConfigFile() {
}

ConfigFile::ConfigFile(const std::string& file_content) {
  reset(file_content);
}

ConfigFile::~ConfigFile() {
}

void ConfigFile::reset(const std::string& file_content) {
  clear();

  std::vector<std::string> lines;
  Tokenize(file_content, "\n", &lines);

  std::vector<std::string> key_value_pair;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].empty())
      continue;

    key_value_pair.clear();
    Tokenize(lines[i], "=", &key_value_pair);
    // Skip lines that don't contain key-value pair and lines without a key.
    if (key_value_pair.size() != 2 || key_value_pair[0].empty())
      continue;

    ProcessLine(key_value_pair);
  }

  // Make sure last block has at least one hwid associated with it.
  DeleteLastBlockIfHasNoHwid();
}

void ConfigFile::clear() {
  config_struct_.clear();
}

const std::string& ConfigFile::GetProperty(
    const std::string& property_name,
    const std::string& hwid) const {
  // We search for block that has desired hwid property, and if we find it, we
  // return its property_name property.
  for (BlockList::const_iterator block_it = config_struct_.begin();
       block_it != config_struct_.end();
       ++block_it) {
    if (block_it->hwids.find(hwid) != block_it->hwids.end()) {
      PropertyMap::const_iterator property =
          block_it->properties.find(property_name);
      if (property != block_it->properties.end()) {
        return property->second;
      } else {
        return EmptyString();
      }
    }
  }

  return EmptyString();
}

// Check if last block has a hwid associated with it, and erase it if it
// doesn't,
void ConfigFile::DeleteLastBlockIfHasNoHwid() {
  if (!config_struct_.empty() && config_struct_.back().hwids.empty()) {
    config_struct_.pop_back();
  }
}

void ConfigFile::ProcessLine(const std::vector<std::string>& line) {
  // If line contains name key, new image block is starting, so we have to add
  // new entry to our data structure.
  if (line[0] == kName) {
    // If there was no hardware class defined for previous block, we can
    // disregard is since we won't be abble to access any of its properties
    // anyway. This should not happen, but let's be defensive.
    DeleteLastBlockIfHasNoHwid();
    config_struct_.resize(config_struct_.size() + 1);
  }

  // If we still haven't added any blocks to data struct, we disregard this
  // line. Again, this should never happen.
  if (config_struct_.empty())
    return;

  ConfigFileBlock& last_block = config_struct_.back();

  if (line[0] == kHwid) {
    // Check if line contains hwid property. If so, add it to set of hwids
    // associated with current block.
    last_block.hwids.insert(line[1]);
  } else {
    // Add new block property.
    last_block.properties.insert(std::make_pair(line[0], line[1]));
  }
}

ConfigFile::ConfigFileBlock::ConfigFileBlock() {
}

ConfigFile::ConfigFileBlock::~ConfigFileBlock() {
}

////////////////////////////////////////////////////////////////////////////////
//
// StateMachine
//
////////////////////////////////////////////////////////////////////////////////
StateMachine::StateMachine()
    : download_started_(false),
      download_finished_(false),
      state_(INITIAL) {
}

StateMachine::~StateMachine() {
}

void StateMachine::OnError(int error_message_id) {
  if (state_ == INITIAL)
    return;
  if (!download_finished_)
    download_started_ = false;

  state_ = INITIAL;
  FOR_EACH_OBSERVER(Observer, observers_, OnError(error_message_id));
}

void StateMachine::OnSuccess() {
  if (state_ == INITIAL)
    return;
  state_ = INITIAL;
  OnStateChanged();
}

////////////////////////////////////////////////////////////////////////////////
//
// BurnManager
//
////////////////////////////////////////////////////////////////////////////////

BurnManager::BurnManager()
    : device_handler_(disks::DiskMountManager::GetInstance()),
      config_file_url_(kConfigFileUrl),
      config_file_fetched_(false),
      state_machine_(new StateMachine()),
      bytes_image_download_progress_last_reported_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  CrosLibrary::Get()->GetBurnLibrary()->AddObserver(this);
  base::WeakPtr<BurnManager> weak_ptr(weak_ptr_factory_.GetWeakPtr());
  device_handler_.SetCallbacks(
      base::Bind(&BurnManager::NotifyDeviceAdded, weak_ptr),
      base::Bind(&BurnManager::NotifyDeviceRemoved, weak_ptr));
}

BurnManager::~BurnManager() {
  if (!image_dir_.empty()) {
    file_util::Delete(image_dir_, true);
  }
  CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

// static
void BurnManager::Initialize() {
  if (g_burn_manager) {
    LOG(WARNING) << "BurnManager was already initialized";
    return;
  }
  g_burn_manager = new BurnManager();
  VLOG(1) << "BurnManager initialized";
}

// static
void BurnManager::Shutdown() {
  if (!g_burn_manager) {
    LOG(WARNING) << "BurnManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_burn_manager;
  g_burn_manager = NULL;
  VLOG(1) << "BurnManager Shutdown completed";
}

// static
BurnManager* BurnManager::GetInstance() {
  return g_burn_manager;
}

void BurnManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BurnManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<disks::DiskMountManager::Disk> BurnManager::GetBurnableDevices() {
  return device_handler_.GetBurnableDevices();
}

bool BurnManager::IsNetworkConnected() const {
  return CrosLibrary::Get()->GetNetworkLibrary()->Connected();
}

void BurnManager::Cancel() {
  OnError(IDS_IMAGEBURN_USER_ERROR);
}

void BurnManager::OnError(int message_id) {
  // If we are in intial state, error has already been dispached.
  if (state_machine_->state() == StateMachine::INITIAL) {
    return;
  }

  // Remember burner state, since it will be reset after OnError call.
  StateMachine::State state = state_machine_->state();

  // Dispach error. All hadlers' OnError event will be called before returning
  // from this. This includes us, too.
  state_machine_->OnError(message_id);

  // Cancel and clean up the current task.
  // Note: the cancellation of this class looks not handled correctly.
  // In particular, there seems no clean-up code for creating a temporary
  // directory, or fetching config files. Also, there seems an issue
  // about the cancellation of BurnLibrary.
  // TODO(hidehiko): Fix the issue.
  if (state  == StateMachine::DOWNLOADING) {
    CancelImageFetch();
  } else if (state == StateMachine::BURNING) {
    // Burn library doesn't send cancelled signal upon CancelBurnImage
    // invokation.
    CancelBurnImage();
  }
  ResetTargetPaths();
}

void BurnManager::CreateImageDir() {
  if (image_dir_.empty()) {
    CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &image_dir_));
    image_dir_ = image_dir_.Append(kTempImageFolderName);
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(CreateDirectory,
                   image_dir_,
                   base::Bind(&BurnManager::OnImageDirCreated,
                              weak_ptr_factory_.GetWeakPtr())));
  } else {
    const bool success = true;
    OnImageDirCreated(success);
  }
}

void BurnManager::OnImageDirCreated(bool success) {
  zip_image_file_path_ = image_dir_.Append(kImageZipFileName);
  FOR_EACH_OBSERVER(Observer, observers_, OnImageDirCreated(success));
}

const base::FilePath& BurnManager::GetImageDir() {
  return image_dir_;
}

void BurnManager::FetchConfigFile() {
  if (config_file_fetched_) {
    FOR_EACH_OBSERVER(Observer, observers_, OnConfigFileFetched(true));
    return;
  }

  if (config_fetcher_.get())
    return;

  config_fetcher_.reset(net::URLFetcher::Create(
      config_file_url_, net::URLFetcher::GET, this));
  config_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  config_fetcher_->Start();
}

void BurnManager::FetchImage() {
  tick_image_download_start_ = base::TimeTicks::Now();
  bytes_image_download_progress_last_reported_ = 0;
  image_fetcher_.reset(net::URLFetcher::Create(image_download_url_,
                                               net::URLFetcher::GET,
                                               this));
  image_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  image_fetcher_->SaveResponseToFileAtPath(
      zip_image_file_path_,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  image_fetcher_->Start();

  state_machine_->OnDownloadStarted();
}

void BurnManager::CancelImageFetch() {
  image_fetcher_.reset();
}

void BurnManager::DoBurn() {
  CrosLibrary::Get()->GetBurnLibrary()->DoBurn(
      zip_image_file_path_, image_file_name_,
      target_file_path(), target_device_path());
  state_machine_->OnBurnStarted();
}

void BurnManager::CancelBurnImage() {
  CrosLibrary::Get()->GetBurnLibrary()->CancelBurnImage();
}

void BurnManager::OnURLFetchComplete(const net::URLFetcher* source) {
  const bool success =
      source->GetStatus().status() == net::URLRequestStatus::SUCCESS;
  if (source == config_fetcher_.get()) {
    std::string data;
    if (success)
      config_fetcher_->GetResponseAsString(&data);
    config_fetcher_.reset();
    ConfigFileFetched(success, data);
  } else if (source == image_fetcher_.get()) {
    state_machine_->OnDownloadFinished();
    FOR_EACH_OBSERVER(Observer, observers_, OnImageFileFetched(success));
  }
}

void BurnManager::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                             int64 current,
                                             int64 total) {
  if (source == image_fetcher_.get()) {
    if (current >= bytes_image_download_progress_last_reported_ +
        kBytesImageDownloadProgressReportInterval) {
      bytes_image_download_progress_last_reported_ = current;
      base::TimeDelta estimated_remaining_time;
      if (current > 0) {
        // Extrapolate from the elapsed time.
        const base::TimeDelta elapsed_time =
            base::TimeTicks::Now() - tick_image_download_start_;
        estimated_remaining_time = elapsed_time * (total - current) / current;
      }
      FOR_EACH_OBSERVER(
          Observer, observers_,
          OnImageFileFetchDownloadProgressUpdated(
              current, total, estimated_remaining_time));
    }
  }
}

void BurnManager::BurnProgressUpdated(BurnLibrary* object,
                                      BurnEvent event,
                                      const ImageBurnStatus& status) {
  if (event == BURN_SUCCESS) {
    // The burning task is successfully done.
    // Update the state.
    state_machine_->OnSuccess();
  }

  // Proxy the BurnLibrary callback to observers.
  FOR_EACH_OBSERVER(
      Observer, observers_, OnBurnProgressUpdated(event, status));
}

void BurnManager::OnNetworkManagerChanged(NetworkLibrary* obj) {
  // TODO(hidehiko): Split this into a class to write tests.
  if (state_machine_->state() == StateMachine::INITIAL && IsNetworkConnected())
    FOR_EACH_OBSERVER(Observer, observers_, OnNetworkDetected());

  if (state_machine_->state() == StateMachine::DOWNLOADING &&
      !IsNetworkConnected())
    OnError(IDS_IMAGEBURN_NETWORK_ERROR);
}

void BurnManager::ConfigFileFetched(bool fetched, const std::string& content) {
  if (config_file_fetched_)
    return;

  // Get image file name and image download URL.
  std::string hwid;
  if (fetched && system::StatisticsProvider::GetInstance()->
      GetMachineStatistic(kHwidStatistic, &hwid)) {
    ConfigFile config_file(content);
    image_file_name_ = config_file.GetProperty(kFileName, hwid);
    image_download_url_ = GURL(config_file.GetProperty(kUrl, hwid));
  }

  // Error check.
  if (fetched && !image_file_name_.empty() && !image_download_url_.is_empty()) {
    config_file_fetched_ = true;
  } else {
    fetched = false;
    image_file_name_.clear();
    image_download_url_ = GURL();
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnConfigFileFetched(fetched));
}

void BurnManager::NotifyDeviceAdded(
    const disks::DiskMountManager::Disk& disk) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceAdded(disk));
}

void BurnManager::NotifyDeviceRemoved(
    const disks::DiskMountManager::Disk& disk) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceRemoved(disk));

  if (target_device_path_.value() == disk.device_path()) {
    // The device is removed during the burning process.
    // Note: in theory, this is not a part of notification, but cancelling
    // the running burning task. However, there is no good place to be in the
    // current code.
    // TODO(hidehiko): Clean this up after refactoring.
    OnError(IDS_IMAGEBURN_DEVICE_NOT_FOUND_ERROR);
  }
}

}  // namespace imageburner
}  // namespace chromeos
