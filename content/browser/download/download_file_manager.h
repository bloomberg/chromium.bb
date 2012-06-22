// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadFileManager owns a set of DownloadFile objects, each of which
// represent one in progress download and performs the disk IO for that
// download. The DownloadFileManager itself is a singleton object owned by the
// ResourceDispatcherHostImpl.
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
// contents (profile) where the download was initiated. In the event of a
// contents closure during a download, the DownloadFileManager will continue to
// route data to the appropriate DownloadManager. In progress downloads are
// cancelled for a DownloadManager that exits (such as when closing a profile).

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
#pragma once

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/base/net_errors.h"
#include "ui/gfx/native_widget_types.h"

struct DownloadCreateInfo;
class DownloadRequestHandle;
class FilePath;

namespace content {
class ByteStreamReader;
class DownloadFile;
class DownloadId;
class DownloadManager;
}

namespace net {
class BoundNetLog;
}

// Manages all in progress downloads.
// Methods are virtual to allow mocks--this class is not intended
// to be a base class.
class CONTENT_EXPORT DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  // Callback used with CreateDownloadFile().  |reason| will be
  // DOWNLOAD_INTERRUPT_REASON_NONE on a successful creation.
  typedef base::Callback<void(content::DownloadInterruptReason reason)>
      CreateDownloadFileCallback;

  // Callback used with RenameInProgressDownloadFile() and
  // RenameCompletingDownloadFile().
  typedef base::Callback<void(const FilePath&)> RenameCompletionCallback;

  class DownloadFileFactory {
   public:
    virtual ~DownloadFileFactory() {}

    virtual content::DownloadFile* CreateFile(
        DownloadCreateInfo* info,
        scoped_ptr<content::ByteStreamReader> stream,
        content::DownloadManager* download_manager,
        bool calculate_hash,
        const net::BoundNetLog& bound_net_log) = 0;
  };

  // Takes ownership of the factory.
  // Passing in a NULL for |factory| will cause a default
  // |DownloadFileFactory| to be used.
  explicit DownloadFileManager(DownloadFileFactory* factory);

  // Create a download file and record it in the download file manager.
  virtual void CreateDownloadFile(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<content::ByteStreamReader> stream,
      scoped_refptr<content::DownloadManager> download_manager,
      bool hash_needed,
      const net::BoundNetLog& bound_net_log,
      const CreateDownloadFileCallback& callback);

  // Called on shutdown on the UI thread.
  virtual void Shutdown();

  // Handlers for notifications sent from the UI thread and run on the
  // FILE thread.  These are both terminal actions with respect to the
  // download file, as far as the DownloadFileManager is concerned -- if
  // anything happens to the download file after they are called, it will
  // be ignored.
  virtual void CancelDownload(content::DownloadId id);
  virtual void CompleteDownload(content::DownloadId id);

  // Called on FILE thread by DownloadManager at the beginning of its shutdown.
  virtual void OnDownloadManagerShutdown(content::DownloadManager* manager);

  // The DownloadManager in the UI thread has provided an intermediate name for
  // the download specified by |id|. |overwrite_existing_file| indicates whether
  // any existing file at |full_path| should be overwritten. If false, adds a
  // uniquifier to |full_path| and uses the resulting name as the intermediate
  // path for the download. Invokes |callback| with the new path on success. If
  // the rename fails, calls CancelDownloadOnRename() and invokes |callback|
  // with an empty FilePath().
  virtual void RenameInProgressDownloadFile(
      content::DownloadId id,
      const FilePath& full_path,
      bool overwrite_existing_file,
      const RenameCompletionCallback& callback);

  // The DownloadManager in the UI thread has provided a final name for the
  // download specified by |id|. |overwrite_existing_file| prevents
  // uniquification, and is used for SAFE downloads, as the user may have
  // decided to overwrite the file.  Sent from the UI thread and run on the FILE
  // thread. Invokes |callback| with the new path on success. If the rename
  // fails, calls CancelDownloadOnRename() and invokes |callback| with an empty
  // FilePath().
  virtual void RenameCompletingDownloadFile(
      content::DownloadId id,
      const FilePath& full_path,
      bool overwrite_existing_file,
      const RenameCompletionCallback& callback);

  // The number of downloads currently active on the DownloadFileManager.
  // Primarily for testing.
  virtual int NumberOfActiveDownloads() const;

  void SetFileFactoryForTesting(scoped_ptr<DownloadFileFactory> file_factory) {
    download_file_factory_.reset(file_factory.release());
  }

  DownloadFileFactory* GetFileFactoryForTesting() const {
    return download_file_factory_.get();  // Explicitly NOT a scoped_ptr.
  }

 protected:
  virtual ~DownloadFileManager();

 private:
  friend class base::RefCountedThreadSafe<DownloadFileManager>;
  friend class DownloadFileManagerTest;
  friend class DownloadManagerTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadManagerTest, StartDownload);

  // Clean up helper that runs on the download thread.
  void OnShutdown();

  // Called only on the download thread.
  content::DownloadFile* GetDownloadFile(content::DownloadId global_id);

  // Called only from RenameInProgressDownloadFile and
  // RenameCompletingDownloadFile on the FILE thread.
  // |rename_error| indicates what error caused the cancel.
  void CancelDownloadOnRename(content::DownloadId global_id,
                              net::Error rename_error);

  // Erases the download file with the given the download |id| and removes
  // it from the maps.
  void EraseDownload(content::DownloadId global_id);

  typedef base::hash_map<content::DownloadId, content::DownloadFile*>
      DownloadFileMap;

  // A map of all in progress downloads.  It owns the download files.
  DownloadFileMap downloads_;

  scoped_ptr<DownloadFileFactory> download_file_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileManager);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_MANAGER_H_
