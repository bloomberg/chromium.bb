// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_WEBUI_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_utils.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace imageburner {

enum ProgressType {
  DOWNLOAD,
  UNZIP,
  BURN
};

class WebUIHandlerTaskProxy
  : public base::RefCountedThreadSafe<WebUIHandlerTaskProxy> {
 public:
  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void CreateImageDirOnFileThread() = 0;
    virtual void ImageDirCreatedOnUIThread(bool success) = 0;
  };

  explicit WebUIHandlerTaskProxy(Delegate* delegate);

  void CreateImageDir() {
    if (delegate_)
      delegate_->CreateImageDirOnFileThread();
  }

  void OnImageDirCreated(bool success) {
    if (delegate_)
      delegate_->ImageDirCreatedOnUIThread(success);
  }

  // WebUIHandlerTaskProxy is created on the UI thread, so in some cases,
  // we need to post back to the UI thread for destruction.
  void DeleteOnUIThread() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebUIHandlerTaskProxy::DoNothing, this));
  }

  void DoNothing() {}

 private:
  base::WeakPtr<Delegate> delegate_;

  friend class base::RefCountedThreadSafe<WebUIHandlerTaskProxy>;
  ~WebUIHandlerTaskProxy();

  DISALLOW_COPY_AND_ASSIGN(WebUIHandlerTaskProxy);
};

class WebUIHandler
    : public WebUIMessageHandler,
      public chromeos::disks::DiskMountManager::Observer,
      public chromeos::BurnLibrary::Observer,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public DownloadItem::Observer,
      public DownloadManager::Observer,
      public Downloader::Listener,
      public StateMachine::Observer,
      public WebUIHandlerTaskProxy::Delegate,
      public BurnManager::Delegate {
 public:
  explicit WebUIHandler(TabContents* contents);
  virtual ~WebUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // chromeos::disks::DiskMountManager::Observer interface.
  virtual void DiskChanged(chromeos::disks::DiskMountManagerEventType event,
                           const chromeos::disks::DiskMountManager::Disk* disk)
      OVERRIDE;
  virtual void DeviceChanged(chromeos::disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE {
  }
  virtual void MountCompleted(
      chromeos::disks::DiskMountManager::MountEvent event_type,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE {
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

  // Downloader::Listener interface.
  virtual void OnBurnDownloadStarted(bool success) OVERRIDE;

  // StateMachine::Observer interface.
  virtual void OnBurnStateChanged(StateMachine::State new_state)
      OVERRIDE;
  virtual void OnError(int error_message_id) OVERRIDE;

 private:
  void CreateDiskValue(const chromeos::disks::DiskMountManager::Disk& disk,
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
  // Part of WebUIHandlerTaskProxy::Delegate interface.
  virtual void CreateImageDirOnFileThread() OVERRIDE;

  // Part of BurnManager::Delegate interface.
  virtual void OnImageDirCreated(bool success)
      OVERRIDE;

  // Part of WebUIHandlerTaskProxy::Delegate interface.
  virtual void ImageDirCreatedOnUIThread(bool success) OVERRIDE;

  // Part of BurnManager::Delegate interface.
  virtual void OnConfigFileFetched(const ConfigFile& config_file, bool success)
      OVERRIDE;

 private:
  void DownloadCompleted(bool success);

  void BurnImage();

  void FinalizeBurn();

  void ProcessError(int message_id);

  bool ExtractInfoFromConfigFile(const ConfigFile& config_file);

  void CleanupDownloadObjects();

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
  DownloadItem*  active_download_item_;
  BurnManager* burn_manager_;
  StateMachine* state_machine_;
  bool observing_burn_lib_;
  bool working_;

  DISALLOW_COPY_AND_ASSIGN(WebUIHandler);
};

}  // namespace imageburner.
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_WEBUI_HANDLER_H_
