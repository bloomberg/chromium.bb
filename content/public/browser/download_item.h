// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Each download is represented by a DownloadItem, and all DownloadItems
// are owned by the DownloadManager which maintains a global list of all
// downloads. DownloadItems are created when a user initiates a download,
// and exist for the duration of the browser life time.
//
// Download observers:
//   DownloadItem::Observer:
//     - allows observers to receive notifications about one download from start
//       to completion
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_
#pragma once

#include <map>
#include <string>

#include "base/string16.h"
#include "content/browser/download/download_state_info.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"

class DownloadFileManager;
class FilePath;
class GURL;
struct DownloadCreateInfo;

namespace base {
class Time;
class TimeDelta;
}

namespace content {

class BrowserContext;
class DownloadId;
class DownloadManager;
class WebContents;
struct DownloadPersistentStoreInfo;

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
//
// This is intended to be used only on the UI thread.
class CONTENT_EXPORT DownloadItem {
 public:
  enum DownloadState {
    // Download is actively progressing.
    IN_PROGRESS = 0,

    // Download is completely finished.
    COMPLETE,

    // Download has been cancelled.
    CANCELLED,

    // This state indicates that the download item is about to be destroyed,
    // and observers seeing this state should release all references.
    REMOVING,

    // This state indicates that the download has been interrupted.
    INTERRUPTED,

    // Maximum value.
    MAX_DOWNLOAD_STATE
  };

  enum SafetyState {
    SAFE = 0,
    DANGEROUS,
    DANGEROUS_BUT_VALIDATED  // Dangerous but the user confirmed the download.
  };

  // Reason for deleting the download.  Passed to Delete().
  enum DeleteReason {
    DELETE_DUE_TO_BROWSER_SHUTDOWN = 0,
    DELETE_DUE_TO_USER_DISCARD
  };

  // A fake download table ID which represents a download that has started,
  // but is not yet in the table.
  static const int kUninitializedHandle;

  static const char kEmptyFileHash[];

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class CONTENT_EXPORT Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) = 0;

    // Called when a downloaded file has been opened.
    virtual void OnDownloadOpened(DownloadItem* download) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Interface for data that can be stored associated with (and owned
  // by) an object of this class via GetExternalData/SetExternalData.
  class ExternalData {
   public:
    virtual ~ExternalData() {};
  };

  virtual ~DownloadItem() {}

  virtual void AddObserver(DownloadItem::Observer* observer) = 0;
  virtual void RemoveObserver(DownloadItem::Observer* observer) = 0;

  // Notifies our observers periodically.
  virtual void UpdateObservers() = 0;

  // Returns true if it is OK to open a folder which this file is inside.
  virtual bool CanShowInFolder() = 0;

  // Returns true if it is OK to register the type of this file so that
  // it opens automatically.
  virtual bool CanOpenDownload() = 0;

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension() = 0;

  // Open the file associated with this download (wait for the download to
  // complete if it is in progress).
  virtual void OpenDownload() = 0;

  // Show the download via the OS shell.
  virtual void ShowDownloadInShell() = 0;

  // Called when the user has validated the download of a dangerous file.
  virtual void DangerousDownloadValidated() = 0;

  // Called periodically from the download thread, or from the UI thread
  // for saving packages.
  // |bytes_so_far| is the number of bytes received so far.
  // |hash_state| is the current hash state.
  virtual void UpdateProgress(int64 bytes_so_far,
                              int64 bytes_per_sec,
                              const std::string& hash_state) = 0;

  // Cancel the download operation. We need to distinguish between cancels at
  // exit (DownloadManager destructor) from user interface initiated cancels
  // because at exit, the history system may not exist, and any updates to it
  // require AddRef'ing the DownloadManager in the destructor which results in
  // a DCHECK failure. Set |user_cancel| to false when canceling from at
  // exit to prevent this crash. This may result in a difference between the
  // downloaded file's size on disk, and what the history system's last record
  // of it is. At worst, we'll end up re-downloading a small portion of the file
  // when resuming a download (assuming the server supports byte ranges).
  virtual void Cancel(bool user_cancel) = 0;

  // Called by external code (SavePackage) using the DownloadItem interface
  // to display progress when the DownloadItem should be considered complete.
  virtual void MarkAsComplete() = 0;

  // Called by the delegate after it delayed opening the download in
  // DownloadManagerDelegate::ShouldOpenDownload.
  virtual void DelayedDownloadOpened() = 0;

  // Called when all data has been saved. Only has display effects.
  virtual void OnAllDataSaved(int64 size, const std::string& final_hash) = 0;

  // Called when the downloaded file is removed.
  virtual void OnDownloadedFileRemoved() = 0;

  // Download operation had an error.
  // |size| is the amount of data received at interruption.
  // |hash_state| is the current hash state at interruption.
  // |reason| is the download interrupt reason code that the operation received.
  virtual void Interrupted(int64 size,
                           const std::string& hash_state,
                           DownloadInterruptReason reason) = 0;

  // Deletes the file from disk and removes the download from the views and
  // history.  |user| should be true if this is the result of the user clicking
  // the discard button, and false if it is being deleted for other reasons like
  // browser shutdown.
  virtual void Delete(DeleteReason reason) = 0;

  // Removes the download from the views and history.
  virtual void Remove() = 0;

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  virtual bool TimeRemaining(base::TimeDelta* remaining) const = 0;

  // Simple speed estimate in bytes/s
  virtual int64 CurrentSpeed() const = 0;

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  virtual int PercentComplete() const = 0;

  // Called when the final path has been determined.
  virtual void OnPathDetermined(const FilePath& path) = 0;

  // Returns true if this download has saved all of its data.
  virtual bool AllDataSaved() const = 0;

  // Update the fields that may have changed in DownloadStateInfo as a
  // result of analyzing the file and figuring out its type, location, etc.
  // May only be called once.
  virtual void SetFileCheckResults(const DownloadStateInfo& state) = 0;

  // Update the download's path, the actual file is renamed on the download
  // thread.
  virtual void Rename(const FilePath& full_path) = 0;

  // Allow the user to temporarily pause a download or resume a paused download.
  virtual void TogglePause() = 0;

  // Called when the download is ready to complete.
  // This may perform final rename if necessary and will eventually call
  // DownloadItem::Completed().
  virtual void OnDownloadCompleting(DownloadFileManager* file_manager) = 0;

  // Called when the file name for the download is renamed to its final name.
  virtual void OnDownloadRenamedToFinalName(const FilePath& full_path) = 0;

  // Returns true if this item matches |query|. |query| must be lower-cased.
  virtual bool MatchesQuery(const string16& query) const = 0;

  // Returns true if the download needs more data.
  virtual bool IsPartialDownload() const = 0;

  // Returns true if the download is still receiving data.
  virtual bool IsInProgress() const = 0;

  // Returns true if the download has been cancelled or was interrupted.
  virtual bool IsCancelled() const = 0;

  // Returns true if the download was interrupted.
  virtual bool IsInterrupted() const = 0;

  // Returns true if we have all the data and know the final file name.
  virtual bool IsComplete() const = 0;

  virtual void SetIsPersisted() = 0;
  virtual bool IsPersisted() const = 0;

  // Accessors
  virtual const std::string& GetHash() const = 0;
  virtual DownloadState GetState() const = 0;
  virtual const FilePath& GetFullPath() const = 0;
  virtual void SetPathUniquifier(int uniquifier) = 0;
  virtual const GURL& GetURL() const = 0;
  virtual const std::vector<GURL>& GetUrlChain() const = 0;
  virtual const GURL& GetOriginalUrl() const = 0;
  virtual const GURL& GetReferrerUrl() const = 0;
  virtual std::string GetSuggestedFilename() const = 0;
  virtual std::string GetContentDisposition() const = 0;
  virtual std::string GetMimeType() const = 0;
  virtual std::string GetOriginalMimeType() const = 0;
  virtual std::string GetReferrerCharset() const = 0;
  virtual std::string GetRemoteAddress() const = 0;
  virtual int64 GetTotalBytes() const = 0;
  virtual void SetTotalBytes(int64 total_bytes) = 0;
  virtual int64 GetReceivedBytes() const = 0;
  virtual const std::string& GetHashState() const = 0;
  virtual int32 GetId() const = 0;
  virtual DownloadId GetGlobalId() const = 0;
  virtual base::Time GetStartTime() const = 0;
  virtual base::Time GetEndTime() const = 0;
  virtual void SetDbHandle(int64 handle) = 0;
  virtual int64 GetDbHandle() const = 0;
  virtual bool IsPaused() const = 0;
  virtual bool GetOpenWhenComplete() const = 0;
  virtual void SetOpenWhenComplete(bool open) = 0;
  virtual bool GetFileExternallyRemoved() const = 0;
  virtual SafetyState GetSafetyState() const = 0;
  // Why |safety_state_| is not SAFE.
  virtual DownloadDangerType GetDangerType() const = 0;
  virtual void SetDangerType(DownloadDangerType danger_type) = 0;
  virtual bool IsDangerous() const = 0;

  virtual bool GetAutoOpened() = 0;
  virtual const FilePath& GetTargetName() const = 0;
  virtual bool PromptUserForSaveLocation() const = 0;
  virtual bool IsOtr() const = 0;
  virtual const FilePath& GetSuggestedPath() const = 0;
  virtual bool IsTemporary() const = 0;
  virtual void SetIsTemporary(bool temporary) = 0;
  virtual void SetOpened(bool opened) = 0;
  virtual bool GetOpened() const = 0;

  virtual const std::string& GetLastModifiedTime() const = 0;
  virtual const std::string& GetETag() const = 0;

  virtual DownloadInterruptReason GetLastReason() const = 0;
  virtual DownloadPersistentStoreInfo GetPersistentStoreInfo() const = 0;
  virtual DownloadStateInfo GetStateInfo() const = 0;
  virtual BrowserContext* GetBrowserContext() const = 0;
  virtual WebContents* GetWebContents() const = 0;

  // Returns the final target file path for the download.
  virtual FilePath GetTargetFilePath() const = 0;

  // Returns the file-name that should be reported to the user. If a display
  // name has been explicitly set using SetDisplayName(), this function returns
  // that display name. Otherwise returns the final target filename.
  virtual FilePath GetFileNameToReportUser() const = 0;

  // Set a display name for the download that will be independent of the target
  // filename. If |name| is not empty, then GetFileNameToReportUser() will
  // return |name|. Has no effect on the final target filename.
  virtual void SetDisplayName(const FilePath& name) = 0;

  // Returns the user-verified target file path for the download.
  // This returns the same path as GetTargetFilePath() for safe downloads
  // but does not for dangerous downloads until the name is verified.
  virtual FilePath GetUserVerifiedFilePath() const = 0;

  // Returns true if the current file name is not the final target name yet.
  virtual bool NeedsRename() const = 0;

  // Cancels the off-thread aspects of the download.
  // TODO(rdsmith): This should be private and only called from
  // DownloadItem::Cancel/Interrupt; it isn't now because we can't
  // call those functions from
  // DownloadManager::FileSelectionCancelled() without doing some
  // rewrites of the DownloadManager queues.
  virtual void OffThreadCancel(DownloadFileManager* file_manager) = 0;

  // Manage data owned by other subsystems associated with the
  // DownloadItem.  By custom, key is the address of a
  // static char subsystem_specific_string[] = ".."; defined
  // in the subsystem, but the only requirement of this interface
  // is that the key be unique over all data stored with this
  // DownloadItem.
  //
  // Note that SetExternalData takes ownership of the
  // passed object; it will be destroyed when the DownloadItem is.
  // If an object is already held by the DownloadItem associated with
  // the passed key, it will be destroyed if overwriten by a new pointer
  // (overwrites by the same pointer are ignored).
  virtual       ExternalData* GetExternalData(const void* key) = 0;
  virtual const ExternalData* GetExternalData(const void* key) const = 0;
  virtual void SetExternalData(const void* key, ExternalData* data) = 0;

  virtual std::string DebugString(bool verbose) const = 0;

  virtual void MockDownloadOpenForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_
