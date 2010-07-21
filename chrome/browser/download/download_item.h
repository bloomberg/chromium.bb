// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"

class DownloadManager;
struct DownloadCreateInfo;

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
class DownloadItem {
 public:
  enum DownloadState {
    IN_PROGRESS,
    COMPLETE,
    CANCELLED,
    REMOVING
  };

  enum SafetyState {
    SAFE = 0,
    DANGEROUS,
    DANGEROUS_BUT_VALIDATED  // Dangerous but the user confirmed the download.
  };

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) = 0;

    // Called when a downloaded file has been completed.
    virtual void OnDownloadFileCompleted(DownloadItem* download) = 0;

    // Called when a downloaded file has been opened.
    virtual void OnDownloadOpened(DownloadItem* download) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Constructing from persistent store:
  explicit DownloadItem(const DownloadCreateInfo& info);

  // Constructing from user action:
  DownloadItem(int32 download_id,
               const FilePath& path,
               int path_uniquifier,
               const GURL& url,
               const GURL& referrer_url,
               const std::string& mime_type,
               const std::string& original_mime_type,
               const FilePath& original_name,
               const base::Time start_time,
               int64 download_size,
               int render_process_id,
               int request_id,
               bool is_dangerous,
               bool save_as,
               bool is_otr,
               bool is_extension_install,
               bool is_temporary);

  ~DownloadItem();

  void Init(bool start_timer);

  // Public API

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies our observers periodically.
  void UpdateObservers();

  // Notifies our observers the downloaded file has been completed.
  void NotifyObserversDownloadFileCompleted();

  // Notifies our observers the downloaded file has been opened.
  void NotifyObserversDownloadOpened();

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

  // Download operation completed.
  void Finished(int64 size);

  // The user wants to remove the download from the views and history. If
  // |delete_file| is true, the file is deleted on the disk.
  void Remove(bool delete_file);

  // Start/stop sending periodic updates to our observers
  void StartProgressTimer();
  void StopProgressTimer();

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

  // Update the download's path, the actual file is renamed on the download
  // thread.
  void Rename(const FilePath& full_path);

  // Allow the user to temporarily pause a download or resume a paused download.
  void TogglePause();

  // Accessors
  DownloadState state() const { return state_; }
  FilePath file_name() const { return file_name_; }
  void set_file_name(const FilePath& name) { file_name_ = name; }
  FilePath full_path() const { return full_path_; }
  void set_full_path(const FilePath& path) { full_path_ = path; }
  int path_uniquifier() const { return path_uniquifier_; }
  void set_path_uniquifier(int uniquifier) { path_uniquifier_ = uniquifier; }
  GURL url() const { return url_; }
  GURL referrer_url() const { return referrer_url_; }
  std::string mime_type() const { return mime_type_; }
  std::string original_mime_type() const { return original_mime_type_; }
  int64 total_bytes() const { return total_bytes_; }
  void set_total_bytes(int64 total_bytes) { total_bytes_ = total_bytes; }
  int64 received_bytes() const { return received_bytes_; }
  int32 id() const { return id_; }
  base::Time start_time() const { return start_time_; }
  void set_db_handle(int64 handle) { db_handle_ = handle; }
  int64 db_handle() const { return db_handle_; }
  DownloadManager* manager() const { return manager_; }
  void set_manager(DownloadManager* manager) { manager_ = manager; }
  bool is_paused() const { return is_paused_; }
  void set_is_paused(bool pause) { is_paused_ = pause; }
  bool open_when_complete() const { return open_when_complete_; }
  void set_open_when_complete(bool open) { open_when_complete_ = open; }
  int render_process_id() const { return render_process_id_; }
  int request_id() const { return request_id_; }
  SafetyState safety_state() const { return safety_state_; }
  void set_safety_state(SafetyState safety_state) {
    safety_state_ = safety_state;
  }
  bool auto_opened() { return auto_opened_; }
  void set_auto_opened(bool auto_opened) { auto_opened_ = auto_opened; }
  FilePath original_name() const { return original_name_; }
  void set_original_name(const FilePath& name) { original_name_ = name; }
  bool save_as() const { return save_as_; }
  bool is_otr() const { return is_otr_; }
  bool is_extension_install() const { return is_extension_install_; }
  bool name_finalized() const { return name_finalized_; }
  void set_name_finalized(bool name_finalized) {
    name_finalized_ = name_finalized;
  }
  bool is_temporary() const { return is_temporary_; }
  void set_is_temporary(bool is_temporary) { is_temporary_ = is_temporary; }
  bool need_final_rename() const { return need_final_rename_; }
  void set_need_final_rename(bool need_final_rename) {
    need_final_rename_ = need_final_rename;
  }

  // Returns the file-name that should be reported to the user, which is
  // file_name_ for safe downloads and original_name_ for dangerous ones with
  // the uniquifier number.
  FilePath GetFileName() const;

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Request ID assigned by the ResourceDispatcherHost.
  int32 id_;

  // Full path to the downloaded file
  FilePath full_path_;

  // A number that should be appended to the path to make it unique, or 0 if the
  // path should be used as is.
  int path_uniquifier_;

  // Short display version of the file
  FilePath file_name_;

  // The URL from whence we came.
  GURL url_;

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
  DownloadManager* manager_;

  // In progress downloads may be paused by the user, we note it here
  bool is_paused_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // Whether the download is considered potentially safe or dangerous
  // (executable files are typically considered dangerous).
  SafetyState safety_state_;

  // Whether the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_;

  // Dangerous download are given temporary names until the user approves them.
  // This stores their original name.
  FilePath original_name_;

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

  // True if the file needs final rename.
  bool need_final_rename_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItem);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_H_
