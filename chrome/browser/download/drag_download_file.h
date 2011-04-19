// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_
#define CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "googleurl/src/gurl.h"
#include "ui/base/dragdrop/download_file_interface.h"

class TabContents;

namespace net {
class FileStream;
}

class DragDownloadFile : public ui::DownloadFileProvider,
                         public DownloadManager::Observer,
                         public DownloadItem::Observer {
 public:
  // On Windows, we need to download into a temporary file. Two threads are
  // involved: background drag-and-drop thread and UI thread.
  // The first parameter file_name_or_path should contain file name while the
  // second parameter file_stream should be NULL.
  //
  // On MacOSX, we need to download into a file stream that has already been
  // created. Only UI thread is involved.
  // The file path and file stream should be provided as the first two
  // parameters.
  DragDownloadFile(const FilePath& file_name_or_path,
                   linked_ptr<net::FileStream> file_stream,
                   const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   TabContents* tab_contents);

  // DownloadFileProvider methods.
  // Called on drag-and-drop thread (Windows).
  // Called on UI thread (MacOSX).
  virtual bool Start(ui::DownloadFileObserver* observer);
  virtual void Stop();
#if defined(OS_WIN)
  virtual IStream* GetStream() { return NULL; }
#endif

  // DownloadManager::Observer methods.
  // Called on UI thread.
  virtual void ModelChanged();

  // DownloadItem::Observer methods.
  // Called on UI thread.
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

 private:
  // Called on drag-and-drop thread (Windows).
  // Called on UI thread (Windows).
  virtual ~DragDownloadFile();

  // Called on drag-and-drop thread (Windows only).
#if defined(OS_WIN)
  void StartNestedMessageLoop();
  void QuitNestedMessageLoop();
#endif

  // Called on either drag-and-drop thread or UI thread (Windows).
  // Called on UI thread (MacOSX).
  void InitiateDownload();
  void DownloadCompleted(bool is_successful);

  // Helper methods to make sure we're in the correct thread.
  void AssertCurrentlyOnDragThread();
  void AssertCurrentlyOnUIThread();

  // Initialized on drag-and-drop thread. Accessed on either thread after that
  // (Windows).
  // Accessed on UI thread (MacOSX).
  FilePath file_path_;
  FilePath file_name_;
  linked_ptr<net::FileStream> file_stream_;
  GURL url_;
  GURL referrer_;
  std::string referrer_encoding_;
  TabContents* tab_contents_;
  MessageLoop* drag_message_loop_;
  FilePath temp_dir_path_;

  // Accessed on drag-and-drop thread (Windows).
  // Accessed on UI thread (MacOSX).
  bool is_started_;
  bool is_successful_;
  scoped_refptr<ui::DownloadFileObserver> observer_;

  // Accessed on drag-and-drop thread (Windows only).
#if defined(OS_WIN)
  bool is_running_nested_message_loop_;
#endif

  // Access on UI thread.
  DownloadManager* download_manager_;
  bool download_item_observer_added_;

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFile);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_
