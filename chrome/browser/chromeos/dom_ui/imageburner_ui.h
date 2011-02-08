// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_IMAGEBURNER_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_IMAGEBURNER_UI_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "ui/base/dragdrop/download_file_interface.h"

template <typename T> struct DefaultSingletonTraits;

class ImageBurnResourceManager;
class TabContents;

class ImageBurnHandler : public WebUIMessageHandler,
                         public chromeos::MountLibrary::Observer,
                         public chromeos::BurnLibrary::Observer,
                         public DownloadManager::Observer,
                         public DownloadItem::Observer,
                         public base::SupportsWeakPtr<ImageBurnHandler> {
 public:
  explicit ImageBurnHandler(TabContents* contents);
  virtual ~ImageBurnHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // chromeos::MountLibrary::Observer interface
  virtual void MountChanged(chromeos::MountLibrary* obj,
                     chromeos::MountEventType evt,
                     const std::string& path);

  // chromeos::BurnLibrary::Observer interface
  virtual void ProgressUpdated(chromeos::BurnLibrary* object,
                               chromeos::BurnEventType evt,
                               const ImageBurnStatus& status);

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadFileCompleted(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download);

  // DownloadManager::Observer interface
  virtual void ModelChanged();

  void CreateImageUrlCallback(GURL* image_url);


 private:
  // Callback for the "getRoots" message.
  void HandleGetRoots(const ListValue* args);

  // Callback for the "downloadImage" message.
  void HandleDownloadImage(const ListValue* args);

  // Callback for the "burnImage" message.
  void HandleBurnImage(const ListValue* args);

  // Callback for the "cancelBurnImage" message.
  void HandleCancelBurnImage(const ListValue* args);

  void DownloadCompleted(bool success);

  void BurnImage();
  void FinalizeBurn(bool successful);

  void UpdateBurnProgress(int64 total_burnt, int64 image_size,
                          const std::string& path, chromeos::BurnEventType evt);
  string16 GetBurnProgressText(int64 total_burnt, int64 image_size);

  // helper functions
  void CreateImageUrl();
  void ExtractTargetedDeviceSystemPath(const ListValue* list_value);
  void CreateLocalImagePath();

 private:
  // file path
  FilePath local_image_file_path_;
  FilePath image_target_;
  GURL* image_download_url_;
  TabContents* tab_contents_;
  DownloadManager* download_manager_;
  bool download_item_observer_added_;
  DownloadItem*  active_download_item_;
  ImageBurnResourceManager* burn_resource_manager_;

  friend class ImageBurnTaskProxy;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnHandler);
};

class ImageBurnResourceManager : public DownloadManager::Observer,
                                 public DownloadItem::Observer {
 public:
  // Returns the singleton instance.
  static ImageBurnResourceManager* GetInstance();

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadFileCompleted(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download);

  // DownloadManager::Observer interface
  virtual void ModelChanged();

  FilePath GetLocalImageDirPath();

  bool CheckImageDownloadStarted();

  void ReportImageDownloadStarted();

  bool CheckDownloadFinished();

  bool CheckBurnInProgress();

  void SetBurnInProgress(bool value);

  void ReportDownloadFinished(bool success);

  void CreateImageUrl(TabContents* tab_content, ImageBurnHandler* downloader);

  void ImageUrlFetched(bool success);

  net::FileStream* CreateFileStream(FilePath* file_path);

 private:
  friend struct DefaultSingletonTraits<ImageBurnResourceManager>;

  ImageBurnResourceManager();
  ~ImageBurnResourceManager();

  FilePath local_image_dir_file_path_;
  FilePath image_fecher_local_path_;
  bool image_download_started_;
  bool image_download_finished_;
  bool burn_in_progress_;
  DownloadManager* download_manager_;
  bool download_item_observer_added_;
  DownloadItem*  active_download_item_;
  scoped_ptr<GURL> image_url_;
  GURL image_fetcher_url_;
  bool image_url_fetching_requested_;
  bool image_url_fetched_;
  std::vector<ImageBurnHandler*> downloaders_;


  DISALLOW_COPY_AND_ASSIGN(ImageBurnResourceManager);
};

class ImageBurnUI : public DOMUI {
 public:
  explicit ImageBurnUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageBurnUI);
};
#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_IMAGEBURNER_UI_H_
