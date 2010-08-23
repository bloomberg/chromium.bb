// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_IMAGEBURNER_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_IMAGEBURNER_UI_H_

#include <string>
#include <vector>

#include "app/download_file_interface.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"

static const std::string kPropertyPath = "path";
static const std::string kPropertyTitle = "title";
static const std::string kPropertyDirectory = "isDirectory";
static const std::string kImageBaseURL =
    "http://chrome-master.mtv.corp.google.com/chromeos/dev-channel/";
static const std::string kImageFetcherName = "LATEST-x86-generic";
static const std::string kImageFileName = "chromeos_image.bin.gz";
static const std::string kTempImageFolderName = "chromeos_image";

class ImageBurnResourceManager;

class ImageBurnUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  ImageBurnUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                 bool is_off_the_record,
                                 int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~ImageBurnUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(ImageBurnUIHTMLSource);
};

class ImageBurnHandler : public DOMMessageHandler,
                         public chromeos::MountLibrary::Observer,
                         public chromeos::BurnLibrary::Observer,
                         public DownloadManager::Observer,
                         public DownloadItem::Observer,
                         public base::SupportsWeakPtr<ImageBurnHandler> {
 public:
  explicit ImageBurnHandler(TabContents* contents);
  virtual ~ImageBurnHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
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
  std::wstring GetBurnProgressText(int64 total_burnt, int64 image_size);

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

class ImageBurnTaskProxy
    : public base::RefCountedThreadSafe<ImageBurnTaskProxy> {
 public:
  explicit ImageBurnTaskProxy(const base::WeakPtr<ImageBurnHandler>& handler);

  bool ReportDownloadInitialized();
  bool CheckDownloadFinished();
  void BurnImage();
  void FinalizeBurn(bool success);

  void CreateImageUrl(TabContents* tab_contents, ImageBurnHandler* downloader);

 private:
  base::WeakPtr<ImageBurnHandler> handler_;
  ImageBurnResourceManager* resource_manager_;

  friend class base::RefCountedThreadSafe<ImageBurnTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnTaskProxy);
};

class ImageBurnResourceManager : public DownloadManager::Observer,
                                 public DownloadItem::Observer {
 public:
  ImageBurnResourceManager();
  ~ImageBurnResourceManager();

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

