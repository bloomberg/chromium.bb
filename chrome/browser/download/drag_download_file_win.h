// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_WIN_H_
#define CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_WIN_H_

#include "app/os_exchange_data.h"
#include "base/file_path.h"
#include "chrome/browser/download/download_manager.h"
#include "googleurl/src/gurl.h"

class TabContents;

class DragDownloadFile : public OSExchangeData::DownloadFileProvider,
                         public DownloadManager::Observer,
                         public DownloadItem::Observer {
 public:
  DragDownloadFile(const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   TabContents* tab_contents);

  // OSExchangeData::DownloadFileProvider methods.
  // Called on drag-and-drop thread.
  virtual bool Start(OSExchangeData::DownloadFileObserver* observer,
                     int format);
  virtual void Stop();

  // DownloadManager::Observer methods.
  // Called on UI thread.
  virtual void ModelChanged();
  virtual void SetDownloads(std::vector<DownloadItem*>& downloads);

  // DownloadItem::Observer methods.
  // Called on UI thread.
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadFileCompleted(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

 private:
  // Called on drag-and-drop thread.
  virtual ~DragDownloadFile();

  bool InitiateDownload();
  void InitiateDownloadSucceeded(
      const std::vector<OSExchangeData::DownloadFileInfo*>& downloads);
  void InitiateDownloadFailed();
  void InitiateDownloadCompleted(bool result);

  void DownloadCancelled();
  void DownloadCompleted(const FilePath& file_path);

  void StartNestedMessageLoop();
  void QuitNestedMessageLoopIfNeeded();

  // Called on UI thread.
  void OnInitiateDownload(const FilePath& dir_path);
  void CheckDownloadStatus(DownloadItem* download);

  // Initialized on drag-and-drop thread. Can be accessed on either thread.
  GURL url_;
  GURL referrer_;
  std::string referrer_encoding_;
  TabContents* tab_contents_;
  MessageLoop* drag_message_loop_;

  // Accessed on drag-and-drop thread.
  bool is_started_;
  bool is_running_nested_message_loop_;
  bool initiate_download_result_;
  scoped_refptr<OSExchangeData::DownloadFileObserver> observer_;
  int format_;
  FilePath dir_path_;
  FilePath file_path_;

  // Access on UI thread.
  DownloadManager* download_manager_;
  bool download_item_observer_added_;

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFile);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_WIN_H_
