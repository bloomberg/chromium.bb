// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "ui/base/dragdrop/download_file_interface.h"


namespace imageburner {

// Config file properties.
extern const char kName[];
extern const char kHwid[];
extern const char kFileName[];
extern const char kUrl[];

class Downloader {
 public:
  class Listener : public base::SupportsWeakPtr<Listener> {
   public:
    // After download starts download status updates can be followed through
    // DownloadItem::Observer interface.
    virtual void OnBurnDownloadStarted(bool success) = 0;
  };

 public:
  Downloader();
  ~Downloader();

  // Downloads a file from the Internet.
  // Should be called from UI thread.
  void DownloadFile(const GURL& url, const FilePath& target_file,
      TabContents* tab_contents);

  // Creates file stream for a download.
  // Must be called from FILE thread.
  void CreateFileStreamOnFileThread(const GURL& url, const FilePath& file_path,
      TabContents* tab_contents);

  // Gets called after file stream is created and starts download.
  void OnFileStreamCreatedOnUIThread(const GURL& url,
      const FilePath& file_path, TabContents* tab_contents,
      net::FileStream* created_file_stream);

  // Adds an item to list of listeners that wait for confirmation that download
  // has started.
  void AddListener(Listener* listener, const GURL& url);

 private:
  // Let listeners know if download started successfully.
  void DownloadStarted(bool success, const GURL& url);

 private:
  typedef std::multimap<GURL, base::WeakPtr<Listener> > ListenerMap;
  ListenerMap listeners_;

  DISALLOW_COPY_AND_ASSIGN(Downloader);
};

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

  bool image_download_requested() const { return image_download_requested_; }
  void OnImageDownloadRequested() { image_download_requested_ = true; }

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
  bool image_download_requested_;
  bool download_started_;
  bool download_finished_;

  State state_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(StateMachine);
};

class BurnManager
    : public DownloadManager::Observer,
      public DownloadItem::Observer,
      public Downloader::Listener {
 public:

  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void OnImageDirCreated(bool success) = 0;
    virtual void OnConfigFileFetched(const ConfigFile& config_file,
        bool success) = 0;
  };

  // Returns the singleton instance.
  static BurnManager* GetInstance();

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE {}

  // DownloadManager::Observer interface.
  virtual void ModelChanged() OVERRIDE;

  // Downloader::Listener interface.
  virtual void OnBurnDownloadStarted(bool success) OVERRIDE;

  // Creates URL image should be fetched from.
  // Must be called from UI thread.
  void FetchConfigFile(TabContents* tab_content,
                       Delegate* delegate);

  // Creates directory image will be downloaded to.
  // Must be called from FILE thread.
  void CreateImageDir(Delegate* delegate);

  // Returns directory image should be dopwnloaded to.
  // The directory has to be previously created.
  const FilePath& GetImageDir();

  void OnConfigFileDownloadedOnFileThread();

  void ConfigFileFetchedOnUIThread(bool fetched, const std::string& content);

  const FilePath& target_device_path() { return target_device_path_; }
  void set_target_device_path(const FilePath& path) {
    target_device_path_ = path;
  }

  const FilePath& target_file_path() { return target_file_path_; }
  void set_target_file_path(const FilePath& path) {
    target_file_path_ = path;
  }

  const FilePath& final_zip_file_path() {return final_zip_file_path_; }
  void set_final_zip_file_path(const FilePath& path) {
    final_zip_file_path_ = path;
  }

  void ResetTargetPaths() {
    target_device_path_.clear();
    target_file_path_.clear();
    final_zip_file_path_.clear();
  }

  StateMachine* state_machine() const { return state_machine_.get(); }

  Downloader* downloader() {
    if (!downloader_.get())
      downloader_.reset(new Downloader());
    return downloader_.get();
  }

 private:
  friend struct DefaultSingletonTraits<BurnManager>;

  BurnManager();
  virtual ~BurnManager();

  FilePath image_dir_;
  FilePath target_device_path_;
  FilePath target_file_path_;
  FilePath config_file_path_;
  FilePath final_zip_file_path_;

  DownloadManager* download_manager_;
  bool download_item_observer_added_;
  DownloadItem*  active_download_item_;

  ConfigFile config_file_;
  GURL config_file_url_;
  bool config_file_requested_;
  bool config_file_fetched_;
  std::vector<base::WeakPtr<Delegate> > downloaders_;

  scoped_ptr<StateMachine> state_machine_;

  scoped_ptr<Downloader> downloader_;

  DISALLOW_COPY_AND_ASSIGN(BurnManager);
};

}  // namespace imageburner.
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UTILS_H_

