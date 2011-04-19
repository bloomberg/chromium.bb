// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
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

namespace net {
class URLRequestContextGetter;
}

// Manages all in progress downloads.
class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  explicit DownloadFileManager(ResourceDispatcherHost* rdh);

  // Called on shutdown on the UI thread.
  void Shutdown();

  // Called on the IO thread
  int GetNextId();

  // Called on UI thread to make DownloadFileManager start the download.
  void StartDownload(DownloadCreateInfo* info);

  // Handlers for notifications sent from the IO thread and run on the
  // FILE thread.
  void UpdateDownload(int id, DownloadBuffer* buffer);
  // |os_error| is 0 for normal completions, and non-0 for errors.
  // |security_info| contains SSL information (cert_id, cert_status,
  // security_bits, ssl_connection_status), which can be used to
  // fine-tune the error message.  It is empty if the transaction
  // was not performed securely.
  void OnResponseCompleted(int id,
                           DownloadBuffer* buffer,
                           int os_error,
                           const std::string& security_info);

  // Handlers for notifications sent from the UI thread and run on the
  // FILE thread.  These are both terminal actions with respect to the
  // download file, as far as the DownloadFileManager is concerned -- if
  // anything happens to the download file after they are called, it will
  // be ignored.
  void CancelDownload(int id);
  void CompleteDownload(int id);

  // Called on FILE thread by DownloadManager at the beginning of its shutdown.
  void OnDownloadManagerShutdown(DownloadManager* manager);

  // The DownloadManager in the UI thread has provided an intermediate
  // .crdownload name for the download specified by |id|.
  void RenameInProgressDownloadFile(int id, const FilePath& full_path);

  // The DownloadManager in the UI thread has provided a final name for the
  // download specified by |id|.
  // |overwrite_existing_file| prevents uniquification, and is used for SAFE
  // downloads, as the user may have decided to overwrite the file.
  // Sent from the UI thread and run on the FILE thread.
  void RenameCompletingDownloadFile(int id,
                                    const FilePath& full_path,
                                    bool overwrite_existing_file);

  // The number of downloads currently active on the DownloadFileManager.
  // Primarily for testing.
  int NumberOfActiveDownloads() const {
    return downloads_.size();
  }

 private:
  friend class base::RefCountedThreadSafe<DownloadFileManager>;
  friend class DownloadManagerTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadManagerTest, StartDownload);

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

  // Called only from RenameInProgressDownloadFile and
  // RenameCompletingDownloadFile on the FILE thread.
  void CancelDownloadOnRename(int id);

  // Erases the download file with the given the download |id| and removes
  // it from the maps.
  void EraseDownload(int id);

  // Unique ID for each DownloadFile.
  int next_id_;

  typedef base::hash_map<int, DownloadFile*> DownloadFileMap;

  // A map of all in progress downloads.  It owns the download files.
  DownloadFileMap downloads_;

  // Schedule periodic updates of the download progress. This timer
  // is controlled from the FILE thread, and posts updates to the UI thread.
  base::RepeatingTimer<DownloadFileManager> update_timer_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
