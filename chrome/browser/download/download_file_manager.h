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
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "gfx/native_widget_types.h"

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

  // Handlers for notifications sent from the IO thread and run on the
  // download thread.
  void StartDownload(DownloadCreateInfo* info);
  void UpdateDownload(int id, DownloadBuffer* buffer);
  void CancelDownload(int id);
  void DownloadFinished(int id, DownloadBuffer* buffer);

  // Called on the UI thread to remove a download item or manager.
  void RemoveDownloadManager(DownloadManager* manager);
  void RemoveDownload(int id, DownloadManager* manager);

#if !defined(OS_MACOSX)
  // The open and show methods run on the file thread, which does not work on
  // Mac OS X (which uses the UI thread for opens).

  // Handler for shell operations sent from the UI to the download thread.
  void OnShowDownloadInShell(const FilePath& full_path);

  // Handler to open or execute a downloaded file.
  void OnOpenDownloadInShell(const FilePath& full_path,
                             const GURL& url,
                             gfx::NativeView parent_window);
#endif

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

  // Handlers for notifications sent from the download thread and run on
  // the UI thread.
  void OnStartDownload(DownloadCreateInfo* info);
  void OnDownloadFinished(int id, int64 bytes_so_far);

  // Called only on UI thread to get the DownloadManager for a tab's profile.
  static DownloadManager* DownloadManagerFromRenderIds(int render_process_id,
                                                       int review_view_id);
  DownloadManager* GetDownloadManager(int download_id);

  // Called only on the download thread.
  DownloadFile* GetDownloadFile(int id);

  // Called on the UI thread to remove a download from the UI progress table.
  void RemoveDownloadFromUIProgress(int id);

  // Called only from OnFinalDownloadName or OnIntermediateDownloadName
  // on the FILE thread.
  void CancelDownloadOnRename(int id);

  // Unique ID for each DownloadFile.
  int next_id_;

  // A map of all in progress downloads.
  typedef base::hash_map<int, DownloadFile*> DownloadFileMap;
  DownloadFileMap downloads_;

  // Throttle updates to the UI thread.
  base::RepeatingTimer<DownloadFileManager> update_timer_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Tracking which DownloadManager to send data to, called only on UI thread.
  // DownloadManagerMap maps download IDs to their DownloadManager.
  typedef base::hash_map<int, DownloadManager*> DownloadManagerMap;
  DownloadManagerMap managers_;

  // RequestMap maps a DownloadManager to all in-progress download IDs.
  // Called only on the UI thread.
  typedef base::hash_set<int> DownloadRequests;
  typedef std::map<DownloadManager*, DownloadRequests> RequestMap;
  RequestMap requests_;

  // Used for progress updates on the UI thread, mapping download->id() to bytes
  // received so far. Written to by the file thread and read by the UI thread.
  typedef base::hash_map<int, int64> ProgressMap;
  ProgressMap ui_progress_;
  Lock progress_lock_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
