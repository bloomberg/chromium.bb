// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_UI_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "ui/base/dragdrop/download_file_interface.h"

enum ProgressType {
  DOWNLOAD,
  UNZIP,
  BURN
};

class ImageBurnDownloader {
 public:
  class Listener : public base::SupportsWeakPtr<Listener> {
   public:
    // After download starts download status updates can be followed through
    // DownloadItem::Observer interface.
    virtual void OnBurnDownloadStarted(bool success) = 0;
  };

 public:
  ImageBurnDownloader();
  ~ImageBurnDownloader();

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

  DISALLOW_COPY_AND_ASSIGN(ImageBurnDownloader);
};

class ImageBurnConfigFile {
 public:
  ImageBurnConfigFile() {}

  explicit ImageBurnConfigFile(const std::string& file_content) {
    reset(file_content);
  }

  void reset(const std::string& file_content);

  void clear();

  bool empty() {
    return config_struct_.empty() && hwids_.empty();
  }

  const std::string& GetProperty(const std::string& property_name) const;

  bool ContainsHwid(const std::string& hwid) const {
    return hwids_.find(hwid) != hwids_.end();
  }

 private:
  std::map<std::string, std::string> config_struct_;
  std::set<std::string> hwids_;
};

class ImageBurnStateMachine {
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

  ImageBurnStateMachine();
  ~ImageBurnStateMachine() {}

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
  bool burn_in_progress_;

  State state_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnStateMachine);
};

class ImageBurnResourceManager
    : public DownloadManager::Observer,
      public DownloadItem::Observer,
      public ImageBurnDownloader::Listener {
 public:

  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void OnImageDirCreated(bool success) = 0;
    virtual void OnConfigFileFetched(const ImageBurnConfigFile& config_file,
        bool success) = 0;
  };

  // Returns the singleton instance.
  static ImageBurnResourceManager* GetInstance();

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE {}

  // DownloadManager::Observer interface.
  virtual void ModelChanged() OVERRIDE;

  // ImageBurnDownloader::Listener interface.
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

  ImageBurnStateMachine* state_machine() const { return state_machine_.get(); }

  ImageBurnDownloader* downloader() {
    if (!downloader_.get())
      downloader_.reset(new ImageBurnDownloader());
    return downloader_.get();
  }

 private:
  friend struct DefaultSingletonTraits<ImageBurnResourceManager>;

  ImageBurnResourceManager();
  virtual ~ImageBurnResourceManager();

  FilePath image_dir_;
  FilePath target_device_path_;
  FilePath target_file_path_;
  FilePath config_file_path_;
  FilePath final_zip_file_path_;

  DownloadManager* download_manager_;
  bool download_item_observer_added_;
  DownloadItem*  active_download_item_;

  ImageBurnConfigFile config_file_;
  GURL config_file_url_;
  bool config_file_requested_;
  bool config_file_fetched_;
  std::vector<base::WeakPtr<Delegate> > downloaders_;

  scoped_ptr<ImageBurnStateMachine> state_machine_;

  scoped_ptr<ImageBurnDownloader> downloader_;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnResourceManager);
};

class ImageBurnTaskProxy
  : public base::RefCountedThreadSafe<ImageBurnTaskProxy> {
 public:
  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void CreateImageDirOnFileThread() = 0;
    virtual void ImageDirCreatedOnUIThread(bool success) = 0;
  };

  explicit ImageBurnTaskProxy(Delegate* delegate) {
    delegate_ = delegate->AsWeakPtr();
    delegate_->DetachFromThread();
  }

  void CreateImageDir() {
    if (delegate_)
      delegate_->CreateImageDirOnFileThread();
  }

  void OnImageDirCreated(bool success) {
    if (delegate_)
      delegate_->ImageDirCreatedOnUIThread(success);
  }

  // ImageBurnTaskProxy is created on the UI thread, so in some cases,
  // we need to post back to the UI thread for destruction.
  void DeleteOnUIThread() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ImageBurnTaskProxy::DoNothing));
  }

  void DoNothing() {}

 private:
  base::WeakPtr<Delegate> delegate_;

  friend class base::RefCountedThreadSafe<ImageBurnTaskProxy>;
  ~ImageBurnTaskProxy() {}

  DISALLOW_COPY_AND_ASSIGN(ImageBurnTaskProxy);
};

class ImageBurnHandler
    : public WebUIMessageHandler,
      public chromeos::MountLibrary::Observer,
      public chromeos::BurnLibrary::Observer,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public DownloadItem::Observer,
      public DownloadManager::Observer,
      public ImageBurnDownloader::Listener,
      public ImageBurnStateMachine::Observer,
      public ImageBurnTaskProxy::Delegate,
      public ImageBurnResourceManager::Delegate {
 public:
  explicit ImageBurnHandler(TabContents* contents);
  virtual ~ImageBurnHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // chromeos::MountLibrary::Observer interface.
  virtual void DiskChanged(chromeos::MountLibraryEventType event,
                           const chromeos::MountLibrary::Disk* disk) OVERRIDE;
  virtual void DeviceChanged(chromeos::MountLibraryEventType event,
                             const std::string& device_path) OVERRIDE {
  }

  // chromeos::BurnLibrary::Observer interface.
  virtual void BurnProgressUpdated(chromeos::BurnLibrary* object,
                                   chromeos::BurnEvent evt,
                                   const ImageBurnStatus& status) OVERRIDE;

  // chromeos::NetworkLibrary::NetworkManagerObserver interface.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj) OVERRIDE;

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE;

  // DownloadManager::Observer interface.
  virtual void ModelChanged() OVERRIDE;

  // ImageBurnDownloader::Listener interface.
  virtual void OnBurnDownloadStarted(bool success) OVERRIDE;

  // ImageBurnStateMachine::Observer interface.
  virtual void OnBurnStateChanged(ImageBurnStateMachine::State new_state)
      OVERRIDE;
  virtual void OnError(int error_message_id) OVERRIDE;

 private:
  void CreateDiskValue(const chromeos::MountLibrary::Disk& disk,
      DictionaryValue* disk_value);

  // Callback for the "getRoots" message.
  void HandleGetDevices(const ListValue* args);

  // Callback for the webuiInitialized message.
  void HandleWebUIInitialized(const ListValue* args);

  // Callback for the "cancelBurnImage" message.
  void HandleCancelBurnImage(const ListValue* args);

  // Callback for the "burnImage" message.
  void HandleBurnImage(const ListValue* args);

 public:
  // Part of ImageBurnTaskProxy::Delegate interface.
  void CreateImageDirOnFileThread() OVERRIDE;

  // Part of ImageBurnResourceManager::Delegate interface.
  virtual void OnImageDirCreated(bool success)
      OVERRIDE;

  // Part of ImageBurnTaskProxy::Delegate interface.
  void ImageDirCreatedOnUIThread(bool success) OVERRIDE;

  // Part of ImageBurnResourceManager::Delegate interface.
  virtual void OnConfigFileFetched(const ImageBurnConfigFile& config_file,
      bool success)
      OVERRIDE;

 private:
  void DownloadCompleted(bool success);

  void BurnImage();

  void FinalizeBurn();

  void ProcessError(int message_id);

  void SendDeviceTooSmallSignal(int64 device_size);

  void SendProgressSignal(ProgressType progress_type, int64 amount_finished,
      int64 amount_total, const base::TimeDelta* time);

  // time_left_text should be previously created.
  void GetProgressTimeLeftText(int message_id,
      const base::TimeDelta* time_left, string16* time_left_text);

  // size_text should be previously created.
  void GetDataSizeText(int64 size, string16* size_text);

  // progress_text should be previously created.
  void GetProgressText(int message_id, int64 amount_finished,
      int64 amount_total, string16* progress_text);

  // device_path has to be previously created.
  void ExtractTargetedDevicePath(const ListValue& list_value, int index,
      FilePath* device_path);

  int64 GetDeviceSize(const std::string& device_path);

  bool CheckNetwork();


 private:
  FilePath zip_image_file_path_;
  GURL image_download_url_;
  std::string image_file_name_;
  TabContents* tab_contents_;
  DownloadManager* download_manager_;
  bool download_item_observer_added_;
  DownloadItem*  active_download_item_;
  ImageBurnResourceManager* resource_manager_;
  ImageBurnStateMachine* state_machine_;
  bool observing_burn_lib_;
  bool working_;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnHandler);
};

class ImageBurnUI : public WebUI {
 public:
  explicit ImageBurnUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageBurnUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_UI_H_
