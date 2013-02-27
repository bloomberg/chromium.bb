// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_MANAGER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/imageburner/burn_device_handler.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
}  // namespace net

namespace chromeos {
namespace imageburner {

// Config file properties.
extern const char kName[];
extern const char kHwid[];
extern const char kFileName[];
extern const char kUrl[];

// Config file is divided into blocks. Each block is associated with one image
// and containes information about that image in form of key-value pairs, one
// pair per line. Each block starts with name property.
// Also, Each image can be associated with multiple hardware classes, so we
// treat hwid property separately.
// Config file example:
//   name=image1
//   version=version1
//   hwid=hwid1
//   hwid=hwid2
//
//   name=name2
//   version=version2
//   hwid=hwid3
class ConfigFile {
 public:
  ConfigFile();

  explicit ConfigFile(const std::string& file_content);

  ~ConfigFile();

  // Builds config file data structure.
  void reset(const std::string& file_content);

  void clear();

  bool empty() const { return config_struct_.empty(); }

  size_t size() const { return config_struct_.size(); }

  // Returns property_name property of image for hardware class hwid.
  const std::string& GetProperty(const std::string& property_name,
                                 const std::string& hwid) const;

 private:
  void DeleteLastBlockIfHasNoHwid();
  void ProcessLine(const std::vector<std::string>& line);

  typedef std::map<std::string, std::string> PropertyMap;
  typedef std::set<std::string> HwidsSet;

  // Struct that contains config file block info. We separate hwid from other
  // properties for two reasons:
  //   * there are multiple hwids defined for each block.
  //   * we will retieve properties by hwid.
  struct ConfigFileBlock {
    ConfigFileBlock();
    ~ConfigFileBlock();

    PropertyMap properties;
    HwidsSet hwids;
  };

  // At the moment we have only two entries in the config file, so we can live
  // with linear search. Should consider changing data structure if number of
  // entries gets bigger.
  // Also, there is only one entry for each hwid, if that changes we should
  // return vector of strings.
  typedef std::list<ConfigFileBlock> BlockList;
  BlockList config_struct_;
};

class StateMachine {
 public:
  enum State {
    INITIAL,
    DOWNLOADING,
    BURNING,
    CANCELLED
  };

  State state() { return state_; }

  class Observer {
   public:
    virtual void OnBurnStateChanged(State new_state) = 0;
    virtual void OnError(int error_message_id) = 0;
  };

  StateMachine();
  ~StateMachine();

  bool download_started() const { return download_started_; }
  void OnDownloadStarted() {
    download_started_ = true;
    state_ = DOWNLOADING;
    OnStateChanged();
  }

  bool download_finished() const { return download_finished_; }
  void OnDownloadFinished() { download_finished_ = true; }

  void OnBurnStarted() {
    state_ = BURNING;
    OnStateChanged();
  }

  bool new_burn_posible() const { return state_ == INITIAL; }

  void OnSuccess();
  void OnError(int error_message_id);
  void OnCancelation();

  void OnStateChanged() {
    FOR_EACH_OBSERVER(Observer, observers_, OnBurnStateChanged(state_));
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  bool download_started_;
  bool download_finished_;

  State state_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(StateMachine);
};

// This is a system-wide singleton class to manage burning the recovery media.
// Here is how the burning image procedure works:
// 0) Choose the device the image to be burned (manually via web-ui).
// 1) Create ImageDir, which is a working directory for the procedure.
// 2) Download the config file.
//   2-1) Fetch the config file from the server.
//   2-2) Parse the config file content, and extract url and name of the image
//        file.
// 3) Fetch the image file.
// 4) Burn the image to the device.
//   4-1) Unzip the fetched image file.
//   4-2) Unmount the device from file system.
//   4-3) Copy the unzipped file to the device directly.
// Currently, this only provides some methods to start/cancel background tasks,
// and some accessors to obtain the current status. Other functions are
// distributed among BurnController and BurnLibrary.
// TODO(hidehiko): Simplify the relationship among this class, BurnLibrary,
// BurnController and helper classes defined above.
class BurnManager : public net::URLFetcherDelegate,
                    public BurnLibrary::Observer,
                    public NetworkLibrary::NetworkManagerObserver {
 public:

  // Interface for classes that need to observe events for the burning image
  // tasks.
  class Observer {
   public:
    // Triggered when a burnable device is added.
    virtual void OnDeviceAdded(const disks::DiskMountManager::Disk& disk) = 0;

    // Triggered when a burnable device is removed.
    virtual void OnDeviceRemoved(const disks::DiskMountManager::Disk& disk) = 0;

    // Triggered when a network is detected.
    virtual void OnNetworkDetected() = 0;

    // Triggered when the creating a ImageDir is done.
    // The status of the creating the directory is passed to |success|.
    virtual void OnImageDirCreated(bool success) = 0;

    // Triggered when the fetching of the config file is done.
    // The result status of the fetch is passed to |success|.
    virtual void OnConfigFileFetched(bool success) = 0;

    // Triggered during the image file downloading periodically.
    // |estimated_remaining_time| is the remaining duration to download the
    // remaining content estimated based on the elapsed time.
    virtual void OnImageFileFetchDownloadProgressUpdated(
        int64 received_bytes,
        int64 total_bytes,
        const base::TimeDelta& estimated_remaining_time) = 0;

    // Triggered when the fetching of the image file is done.
    // The result status is passed to |success|.
    virtual void OnImageFileFetched(bool success) = 0;

    // Triggered during the burning the image to the device.
    // See also BurnLibrary's comment for more details.
    virtual void OnBurnProgressUpdated(BurnEvent event,
                                       const ImageBurnStatus& status) = 0;
  };

  // Creates the global BurnManager instance.
  static void Initialize();

  // Destroys the global BurnManager instance if it exists.
  static void Shutdown();

  // Returns the global BurnManager instance.
  // Initialize() should already have been called.
  static BurnManager* GetInstance();

  // Add an observer.
  void AddObserver(Observer* observer);

  // Remove an observer.
  void RemoveObserver(Observer* observer);

  // Returns devices on which we can burn recovery image.
  std::vector<disks::DiskMountManager::Disk> GetBurnableDevices();

  // Returns true if some network is connected.
  bool IsNetworkConnected() const;

  // Error is usually detected by all existing Burn handlers, but only first
  // one that calls this method should actually process it.
  // The |message_id| is the id for human readable error message, although
  // here is not the place to handle UI.
  // TODO(hidehiko): Replace it with semantical enum value.
  // Note: currently, due to some implementation reasons, the errors can be
  // observed in outside classes, and this method is public to be accessed from
  // them.
  // TODO(hidehiko): Refactor the structure.
  void OnError(int message_id);

  // Creates URL image should be fetched from.
  // Must be called from UI thread.
  void FetchConfigFile();

  // Fetch a zipped recovery image.
  void FetchImage();

  // Burns the image of |zip_image_file_path_| and |image_file_name|
  // to |target_device_path_| and |target_file_path_|.
  void DoBurn();

  // Cancels the image burning.
  void CancelBurnImage();

  // Cancel fetching image.
  void CancelImageFetch();

  // URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;

  // BurnLibrary::Observer orverrides.
  virtual void BurnProgressUpdated(BurnLibrary* object,
                                   BurnEvent event,
                                   const ImageBurnStatus& status) OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver interface.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;

  // Creates directory image will be downloaded to.
  // Must be called from FILE thread.
  void CreateImageDir();

  // Returns directory image should be dopwnloaded to.
  // The directory has to be previously created.
  const base::FilePath& GetImageDir();

  const base::FilePath& target_device_path() { return target_device_path_; }
  void set_target_device_path(const base::FilePath& path) {
    target_device_path_ = path;
  }

  const base::FilePath& target_file_path() { return target_file_path_; }
  void set_target_file_path(const base::FilePath& path) {
    target_file_path_ = path;
  }

  void ResetTargetPaths() {
    target_device_path_.clear();
    target_file_path_.clear();
  }

  StateMachine* state_machine() const { return state_machine_.get(); }

 private:
  BurnManager();
  virtual ~BurnManager();

  void OnImageDirCreated(bool success);
  void ConfigFileFetched(bool fetched, const std::string& content);

  void NotifyDeviceAdded(const disks::DiskMountManager::Disk& disk);
  void NotifyDeviceRemoved(const disks::DiskMountManager::Disk& disk);

  BurnDeviceHandler device_handler_;

  base::FilePath image_dir_;
  base::FilePath zip_image_file_path_;
  base::FilePath target_device_path_;
  base::FilePath target_file_path_;

  GURL config_file_url_;
  bool config_file_fetched_;
  std::string image_file_name_;
  GURL image_download_url_;

  scoped_ptr<StateMachine> state_machine_;

  scoped_ptr<net::URLFetcher> config_fetcher_;
  scoped_ptr<net::URLFetcher> image_fetcher_;

  base::TimeTicks tick_image_download_start_;
  int64 bytes_image_download_progress_last_reported_;

  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BurnManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BurnManager);
};

}  // namespace imageburner

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_MANAGER_H_
