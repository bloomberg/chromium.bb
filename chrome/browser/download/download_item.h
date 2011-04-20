// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"

class DownloadFileManager;
class DownloadManager;
struct DownloadCreateInfo;

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
//
// This is intended to be used only on the UI thread.
class DownloadItem {
 public:
  enum DownloadState {
    // Download is actively progressing.
    IN_PROGRESS,

    // Download is completely finished.
    COMPLETE,

    // Download has been cancelled.
    CANCELLED,

    // This state indicates that the download item is about to be destroyed,
    // and observers seeing this state should release all references.
    REMOVING,

    // This state indicates that the download has been interrupted.
    INTERRUPTED
  };

  enum SafetyState {
    SAFE = 0,
    DANGEROUS,
    DANGEROUS_BUT_VALIDATED  // Dangerous but the user confirmed the download.
  };

  // This enum is used by histograms.  Do not change the ordering or remove
  // items.
  enum DangerType {
    NOT_DANGEROUS = 0,

    // A dangerous file to the system (e.g.: an executable or extension from
    // places other than gallery).
    DANGEROUS_FILE,

    // Safebrowsing service shows this URL leads to malicious file download.
    DANGEROUS_URL,

    // Memory space for histograms is determined by the max.
    // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
    DANGEROUS_TYPE_MAX
  };

  // Reason for deleting the download.  Passed to Delete().
  enum DeleteReason {
    DELETE_DUE_TO_BROWSER_SHUTDOWN = 0,
    DELETE_DUE_TO_USER_DISCARD
  };

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) = 0;

    // Called when a downloaded file has been opened.
    virtual void OnDownloadOpened(DownloadItem* download) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Constructing from persistent store:
  DownloadItem(DownloadManager* download_manager,
               const DownloadCreateInfo& info);

  // Constructing for a regular download:
  DownloadItem(DownloadManager* download_manager,
               const DownloadCreateInfo& info,
               bool is_otr);

  // Constructing for the "Save Page As..." feature:
  DownloadItem(DownloadManager* download_manager,
               const FilePath& path,
               const GURL& url,
               bool is_otr);

  ~DownloadItem();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies our observers periodically.
  void UpdateObservers();

  // Whether it is OK to open this download.
  bool CanOpenDownload();

  // Tests if a file type should be opened automatically.
  bool ShouldOpenFileBasedOnExtension();

  // Registers this file extension for automatic opening upon download
  // completion if 'open' is true, or prevents the extension from automatic
  // opening if 'open' is false.
  void OpenFilesBasedOnExtension(bool open);

  // Open the file associated with this download (wait for the download to
  // complete if it is in progress).
  void OpenDownload();

  // Show the download via the OS shell.
  void ShowDownloadInShell();

  // Called when the user has validated the download of a dangerous file.
  void DangerousDownloadValidated();

  // Received a new chunk of data
  void Update(int64 bytes_so_far);

  // Cancel the download operation. We need to distinguish between cancels at
  // exit (DownloadManager destructor) from user interface initiated cancels
  // because at exit, the history system may not exist, and any updates to it
  // require AddRef'ing the DownloadManager in the destructor which results in
  // a DCHECK failure. Set 'update_history' to false when canceling from at
  // exit to prevent this crash. This may result in a difference between the
  // downloaded file's size on disk, and what the history system's last record
  // of it is. At worst, we'll end up re-downloading a small portion of the file
  // when resuming a download (assuming the server supports byte ranges).
  void Cancel(bool update_history);

  // Called when all data has been saved.  Only has display effects.
  void OnAllDataSaved(int64 size);

  // Called by external code (SavePackage) using the DownloadItem interface
  // to display progress when the DownloadItem should be considered complete.
  void MarkAsComplete();

  // Download operation had an error.
  // |size| is the amount of data received so far, and |os_error| is the error
  // code that the operation received.
  void Interrupted(int64 size, int os_error);

  // Deletes the file from disk and removes the download from the views and
  // history.  |user| should be true if this is the result of the user clicking
  // the discard button, and false if it is being deleted for other reasons like
  // browser shutdown.
  void Delete(DeleteReason reason);

  // Removes the download from the views and history.
  void Remove();

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  bool TimeRemaining(base::TimeDelta* remaining) const;

  // Simple speed estimate in bytes/s
  int64 CurrentSpeed() const;

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete() const;

  // Whether or not this download has saved all of its data.
  bool all_data_saved() const { return all_data_saved_; }

  // Update the fields that may have changed in DownloadCreateInfo as a
  // result of analyzing the file and figuring out its type, location, etc.
  // May only be called once.
  void SetFileCheckResults(const FilePath& path,
                           bool is_dangerous_file,
                           bool is_dangerous_url,
                           int path_uniquifier,
                           bool prompt,
                           bool is_extension_install,
                           const FilePath& original_name);

  // Update the download's path, the actual file is renamed on the download
  // thread.
  void Rename(const FilePath& full_path);

  // Allow the user to temporarily pause a download or resume a paused download.
  void TogglePause();

  // Called when the name of the download is finalized.
  void OnNameFinalized();

  // Called when the download is ready to complete.
  // This may perform final rename if necessary and will eventually call
  // DownloadItem::Completed().
  void OnDownloadCompleting(DownloadFileManager* file_manager);

  // Called when the file name for the download is renamed to its final name.
  void OnDownloadRenamedToFinalName(const FilePath& full_path);

  // Returns true if this item matches |query|. |query| must be lower-cased.
  bool MatchesQuery(const string16& query) const;

  // Returns true if the download needs more data.
  bool IsPartialDownload() const;

  // Returns true if the download is still receiving data.
  bool IsInProgress() const;

  // Returns true if the download has been cancelled or was interrupted.
  bool IsCancelled() const;

  // Returns true if the download was interrupted.
  bool IsInterrupted() const;

  // Returns true if we have all the data and know the final file name.
  bool IsComplete() const;

  // Accessors
  DownloadState state() const { return state_; }
  FilePath full_path() const { return full_path_; }
  void set_path_uniquifier(int uniquifier) { path_uniquifier_ = uniquifier; }
  const GURL& url() const { return url_chain_.back(); }
  const std::vector<GURL>& url_chain() const { return url_chain_; }
  const GURL& original_url() const { return url_chain_.front(); }
  const GURL& referrer_url() const { return referrer_url_; }
  std::string mime_type() const { return mime_type_; }
  std::string original_mime_type() const { return original_mime_type_; }
  int64 total_bytes() const { return total_bytes_; }
  void set_total_bytes(int64 total_bytes) { total_bytes_ = total_bytes; }
  int64 received_bytes() const { return received_bytes_; }
  int32 id() const { return id_; }
  base::Time start_time() const { return start_time_; }
  void set_db_handle(int64 handle) { db_handle_ = handle; }
  int64 db_handle() const { return db_handle_; }
  bool is_paused() const { return is_paused_; }
  bool open_when_complete() const { return open_when_complete_; }
  void set_open_when_complete(bool open) { open_when_complete_ = open; }
  int render_process_id() const { return render_process_id_; }
  int request_id() const { return request_id_; }
  SafetyState safety_state() const { return safety_state_; }
  void set_safety_state(SafetyState safety_state) {
    safety_state_ = safety_state;
  }
  DangerType danger_type() { return danger_type_;}
  bool auto_opened() { return auto_opened_; }
  FilePath target_name() const { return target_name_; }
  bool save_as() const { return save_as_; }
  bool is_otr() const { return is_otr_; }
  bool is_extension_install() const { return is_extension_install_; }
  bool name_finalized() const { return name_finalized_; }
  bool is_temporary() const { return is_temporary_; }
  void set_opened(bool opened) { opened_ = opened; }
  bool opened() const { return opened_; }

  // Returns the final target file path for the download.
  FilePath GetTargetFilePath() const;

  // Returns the file-name that should be reported to the user, which is
  // target_name_ possibly with the uniquifier number.
  FilePath GetFileNameToReportUser() const;

  // Returns the user-verified target file path for the download.
  // This returns the same path as GetTargetFilePath() for safe downloads
  // but does not for dangerous downloads until the name is verified.
  FilePath GetUserVerifiedFilePath() const;

  // Returns true if the current file name is not the final target name yet.
  bool NeedsRename() const {
    return target_name_ != full_path_.BaseName();
  }

  std::string DebugString(bool verbose) const;

 private:
  void Init(bool start_timer);

  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Called when the entire download operation (including renaming etc)
  // is completed.
  void Completed();

  // Start/stop sending periodic updates to our observers
  void StartProgressTimer();
  void StopProgressTimer();

  // Request ID assigned by the ResourceDispatcherHost.
  int32 id_;

  // Full path to the downloaded or downloading file.
  FilePath full_path_;

  // A number that should be appended to the path to make it unique, or 0 if the
  // path should be used as is.
  int path_uniquifier_;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

  // The mimetype of the download
  std::string mime_type_;

  // The value of the content type header received when downloading
  // this item.  |mime_type_| may be different because of type sniffing.
  std::string original_mime_type_;

  // Total bytes expected
  int64 total_bytes_;

  // Current received bytes
  int64 received_bytes_;

  // Last error.
  int last_os_error_;

  // Start time for calculating remaining time
  base::TimeTicks start_tick_;

  // The current state of this download
  DownloadState state_;

  // The views of this item in the download shelf and download tab
  ObserverList<Observer> observers_;

  // Time the download was started
  base::Time start_time_;

  // Our persistent store handle
  int64 db_handle_;

  // Timer for regularly updating our observers
  base::RepeatingTimer<DownloadItem> update_timer_;

  // Our owning object
  DownloadManager* download_manager_;

  // In progress downloads may be paused by the user, we note it here
  bool is_paused_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // Whether the download is considered potentially safe or dangerous
  // (executable files are typically considered dangerous).
  SafetyState safety_state_;

  // Why |safety_state_| is not SAFE.
  DangerType danger_type_;

  // Whether the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_;

  // Dangerous downloads or ongoing downloads are given temporary names until
  // the user approves them or the downloads finish.
  // This stores their final target name.
  FilePath target_name_;

  // For canceling or pausing requests.
  int render_process_id_;
  int request_id_;

  // True if the item was downloaded as a result of 'save as...'
  bool save_as_;

  // True if the download was initiated in an incognito window.
  bool is_otr_;

  // True if the item was downloaded for an extension installation.
  bool is_extension_install_;

  // True if the filename is finalized.
  bool name_finalized_;

  // True if the item was downloaded temporarily.
  bool is_temporary_;

  // True if we've saved all the data for the download.
  bool all_data_saved_;

  // Did the user open the item either directly or indirectly (such as by
  // setting always open files of this type)? The shelf also sets this field
  // when the user closes the shelf before the item has been opened but should
  // be treated as though the user opened it.
  bool opened_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItem);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_H_
