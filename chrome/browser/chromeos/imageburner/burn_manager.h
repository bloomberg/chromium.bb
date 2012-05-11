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

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

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

class BurnManager : content::URLFetcherDelegate {
 public:

  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void OnImageDirCreated(bool success) = 0;
    virtual void OnConfigFileFetched(bool success,
                                     const std::string& image_file_name,
                                     const GURL& image_download_url) = 0;
  };

  class Observer {
   public:
    virtual void OnDownloadUpdated(int64 received_bytes,
                                   int64 total_bytes,
                                   const base::TimeDelta& time_remaining) = 0;
    virtual void OnDownloadCancelled() = 0;
    virtual void OnDownloadCompleted() = 0;
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

  // Creates URL image should be fetched from.
  // Must be called from UI thread.
  void FetchConfigFile(Delegate* delegate);

  // Fetch a zipped recovery image.
  void FetchImage(const GURL& image_url, const FilePath& file_path);

  // Cancel fetching image.
  void CancelImageFetch();

  // URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;

  // Creates directory image will be downloaded to.
  // Must be called from FILE thread.
  void CreateImageDir(Delegate* delegate);

  // Returns directory image should be dopwnloaded to.
  // The directory has to be previously created.
  const FilePath& GetImageDir();

  const FilePath& target_device_path() { return target_device_path_; }
  void set_target_device_path(const FilePath& path) {
    target_device_path_ = path;
  }

  const FilePath& target_file_path() { return target_file_path_; }
  void set_target_file_path(const FilePath& path) {
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

  void OnImageDirCreated(Delegate* delegate, bool success);
  void ConfigFileFetched(bool fetched, const std::string& content);

  base::WeakPtrFactory<BurnManager> weak_ptr_factory_;

  FilePath image_dir_;
  FilePath target_device_path_;
  FilePath target_file_path_;

  GURL config_file_url_;
  bool config_file_fetched_;
  std::string image_file_name_;
  GURL image_download_url_;
  std::vector<base::WeakPtr<Delegate> > downloaders_;

  scoped_ptr<StateMachine> state_machine_;

  scoped_ptr<content::URLFetcher> config_fetcher_;
  scoped_ptr<content::URLFetcher> image_fetcher_;

  base::TimeTicks tick_image_download_start_;
  int64 bytes_image_download_progress_last_reported_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BurnManager);
};

}  // namespace imageburner

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_MANAGER_H_
