// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadFileManager owns a set of DownloadFile objects, each of which
// represent one in progress download and performs the disk IO for that
// download. The DownloadFileManager itself is a singleton object owned by the
// ResourceDispatcherHost.
//
// The DownloadFileManager uses the file_thread for performing file write
// operations, in order to avoid disk activity on either the IO (network) thread
// and the UI thread. It coordinates the notifications from the network and UI.
//
// A typical download operation involves multiple threads:
//
// Updating an in progress download
// io_thread
//      |----> data ---->|
//                     file_thread (writes to disk)
//                              |----> stats ---->|
//                                              ui_thread (feedback for user and
//                                                         updates to history)
//
// Cancel operations perform the inverse order when triggered by a user action:
// ui_thread (user click)
//    |----> cancel command ---->|
//                          file_thread (close file)
//                                 |----> cancel command ---->|
//                                                    io_thread (stops net IO
//                                                               for download)
//
// The DownloadFileManager tracks download requests, mapping from a download
// ID (unique integer created in the IO thread) to the DownloadManager for the
// tab (profile) where the download was initiated. In the event of a tab closure
// during a download, the DownloadFileManager will continue to route data to the
// appropriate DownloadManager. In progress downloads are cancelled for a
// DownloadManager that exits (such as when closing a profile).

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "ui/gfx/native_widget_types.h"

struct DownloadBuffer;
struct DownloadCreateInfo;
struct DownloadSaveInfo;
class DownloadFile;
class DownloadManager;
class FilePath;
class GURL;
class ResourceDispatcherHost;
class URLRequestContextGetter;

// Manages all in progress downloads.
class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {

  // For testing.
  friend class DownloadManagerTest;

 public:
  explicit DownloadFileManager(ResourceDispatcherHost* rdh);

  // Called on shutdown on the UI thread.
  void Shutdown();

  // Called on the IO thread
  int GetNextId();

  // Called on UI thread to make DownloadFileManager start the download.
  void StartDownload(DownloadCreateInfo* info);

  // Handlers for notifications sent from the IO thread and run on the
  // download thread.
  void UpdateDownload(int id, DownloadBuffer* buffer);
  void CancelDownload(int id);
  void OnResponseCompleted(int id, DownloadBuffer* buffer);

  // Called on FILE thread by DownloadManager at the beginning of its shutdown.
  void OnDownloadManagerShutdown(DownloadManager* manager);

  // The DownloadManager in the UI thread has provided an intermediate
  // .crdownload name for the download specified by 'id'.
  void OnIntermediateDownloadName(int id, const FilePath& full_path,
                                  DownloadManager* download_manager);

  // The download manager has provided a final name for a download. Sent from
  // the UI thread and run on the download thread.
  // |need_delete_crdownload| indicates if we explicitly delete the intermediate
  // .crdownload file or not.
  void OnFinalDownloadName(int id, const FilePath& full_path,
                           bool need_delete_crdownload,
                           DownloadManager* download_manager);

 private:
  friend class base::RefCountedThreadSafe<DownloadFileManager>;

  ~DownloadFileManager();

  // Timer helpers for updating the UI about the current progress of a download.
  void StartUpdateTimer();
  void StopUpdateTimer();
  void UpdateInProgressDownloads();

  // Clean up helper that runs on the download thread.
  void OnShutdown();

  // Creates DownloadFile on FILE thread and continues starting the download
  // process.
  void CreateDownloadFile(DownloadCreateInfo* info,
                          DownloadManager* download_manager,
                          bool hash_needed);

  // Tells the ResourceDispatcherHost to resume a download request
  // that was paused to wait for the on-disk file to be created.
  void ResumeDownloadRequest(int child_id, int request_id);

  // Called only on the download thread.
  DownloadFile* GetDownloadFile(int id);

  // Called only from OnFinalDownloadName or OnIntermediateDownloadName
  // on the FILE thread.
  void CancelDownloadOnRename(int id);

  // Unique ID for each DownloadFile.
  int next_id_;

  // A map of all in progress downloads.
  typedef base::hash_map<int, DownloadFile*> DownloadFileMap;
  DownloadFileMap downloads_;

  // Schedule periodic updates of the download progress. This timer
  // is controlled from the FILE thread, and posts updates to the UI thread.
  base::RepeatingTimer<DownloadFileManager> update_timer_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
