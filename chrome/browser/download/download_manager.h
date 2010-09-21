// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active profile in Chrome.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/shell_dialogs.h"

class DownloadFileManager;
class DownloadHistory;
class DownloadItem;
class DownloadPrefs;
class GURL;
class Profile;
class ResourceDispatcherHost;
class URLRequestContextGetter;
class TabContents;
struct DownloadCreateInfo;
struct DownloadSaveInfo;

// Browser's download manager: manages all downloads and destination view.
class DownloadManager
    : public base::RefCountedThreadSafe<DownloadManager,
                                        ChromeThread::DeleteOnUIThread>,
      public SelectFileDialog::Listener {
  // For testing.
  friend class DownloadManagerTest;
  friend class MockDownloadManager;

 public:
  DownloadManager();

  // Shutdown the download manager. Must be called before destruction.
  void Shutdown();

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class Observer {
   public:
    // New or deleted download, observers should query us for the current set
    // of downloads.
    virtual void ModelChanged() = 0;

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown() {}

   protected:
    virtual ~Observer() {}
  };

  // Return all temporary downloads that reside in the specified directory.
  void GetTemporaryDownloads(const FilePath& dir_path,
                             std::vector<DownloadItem*>* result);

  // Return all non-temporary downloads in the specified directory that are
  // are in progress or have finished.
  void GetAllDownloads(const FilePath& dir_path,
                       std::vector<DownloadItem*>* result);

  // Return all non-temporary downloads in the specified directory that are
  // either in-progress or finished but still waiting for user confirmation.
  void GetCurrentDownloads(const FilePath& dir_path,
                           std::vector<DownloadItem*>* result);

  // Returns all non-temporary downloads matching |query|. Empty query matches
  // everything.
  void SearchDownloads(const string16& query,
                       std::vector<DownloadItem*>* result);

  // Returns true if initialized properly.
  bool Init(Profile* profile);

  // Notifications sent from the download thread to the UI thread
  void StartDownload(DownloadCreateInfo* info);
  void UpdateDownload(int32 download_id, int64 size);
  void OnAllDataSaved(int32 download_id, int64 size);

  // Called from a view when a user clicks a UI button or link.
  void DownloadCancelled(int32 download_id);
  void PauseDownload(int32 download_id, bool pause);
  void RemoveDownload(int64 download_handle);

  // Called when the download is renamed to its final name.
  void DownloadRenamedToFinalName(int download_id, const FilePath& full_path);

  // Remove downloads after remove_begin (inclusive) and before remove_end
  // (exclusive). You may pass in null Time values to do an unbounded delete
  // in either direction.
  int RemoveDownloadsBetween(const base::Time remove_begin,
                             const base::Time remove_end);

  // Remove downloads will delete all downloads that have a timestamp that is
  // the same or more recent than |remove_begin|. The number of downloads
  // deleted is returned back to the caller.
  int RemoveDownloads(const base::Time remove_begin);

  // Remove all downloads will delete all downloads. The number of downloads
  // deleted is returned back to the caller.
  int RemoveAllDownloads();

  // Called when a Save Page As download is started.
  void SavePageAsDownloadStarted(DownloadItem* download_item);

  // Download the object at the URL. Used in cases such as "Save Link As..."
  void DownloadUrl(const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   TabContents* tab_contents);

  // Download the object at the URL and save it to the specified path. The
  // download is treated as the temporary download and thus will not appear
  // in the download history. Used in cases such as drag and drop.
  void DownloadUrlToFile(const GURL& url,
                         const GURL& referrer,
                         const std::string& referrer_encoding,
                         const DownloadSaveInfo& save_info,
                         TabContents* tab_contents);

  // Allow objects to observe the download creation process.
  void AddObserver(Observer* observer);

  // Remove a download observer from ourself.
  void RemoveObserver(Observer* observer);

  // Methods called on completion of a query sent to the history system.
  void OnQueryDownloadEntriesComplete(
      std::vector<DownloadCreateInfo>* entries);
  void OnCreateDownloadEntryComplete(DownloadCreateInfo info, int64 db_handle);

  // Display a new download in the appropriate browser UI.
  void ShowDownloadInBrowser(const DownloadCreateInfo& info,
                             DownloadItem* download);

  // The number of in progress (including paused) downloads.
  int in_progress_count() const {
    return static_cast<int>(in_progress_.size());
  }

  Profile* profile() { return profile_; }

  DownloadHistory* download_history() { return download_history_.get(); }

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }

  // Clears the last download path, used to initialize "save as" dialogs.
  void ClearLastDownloadPath();

  // Tests if a file type should be opened automatically.
  bool ShouldOpenFileBasedOnExtension(const FilePath& path) const;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Called when the user has validated the download of a dangerous file.
  void DangerousDownloadValidated(DownloadItem* download);

 private:
  // This class is used to let an incognito DownloadManager observe changes to
  // a normal DownloadManager, to propagate ModelChanged() calls from the parent
  // DownloadManager to the observers of the incognito DownloadManager.
  class OtherDownloadManagerObserver : public Observer {
   public:
    explicit OtherDownloadManagerObserver(
        DownloadManager* observing_download_manager);
    virtual ~OtherDownloadManagerObserver();

    // Observer interface.
    virtual void ModelChanged();
    virtual void ManagerGoingDown();

   private:
    // The incognito download manager.
    DownloadManager* observing_download_manager_;

    // The original profile's download manager.
    DownloadManager* observed_download_manager_;
  };

  friend class ChromeThread;
  friend class DeleteTask<DownloadManager>;
  friend class OtherDownloadManagerObserver;

  ~DownloadManager();

  // Called on the download thread to check whether the suggested file path
  // exists.  We don't check if the file exists on the UI thread to avoid UI
  // stalls from interacting with the file system.
  void CheckIfSuggestedPathExists(DownloadCreateInfo* info);

  // Called on the UI thread once the DownloadManager has determined whether the
  // suggested file path exists.
  void OnPathExistenceAvailable(DownloadCreateInfo* info);

  // Called back after a target path for the file to be downloaded to has been
  // determined, either automatically based on the suggested file name, or by
  // the user in a Save As dialog box.
  void CreateDownloadItem(DownloadCreateInfo* info,
                          const FilePath& target_path);

  // Download cancel helper function.
  void DownloadCancelledInternal(int download_id,
                                 int render_process_id,
                                 int request_id);

  // Performs the last steps required when a download has been completed.
  // It is necessary to break down the flow when a download is finished as
  // dangerous downloads are downloaded to temporary files that need to be
  // renamed on the file thread first.
  // Invoked on the UI thread.
  // Marked virtual for testing.
  virtual void ContinueDownloadFinished(DownloadItem* download);

  // Renames a finished dangerous download from its temporary file name to its
  // real file name.
  // Invoked on the file thread.
  void ProceedWithFinishedDangerousDownload(int64 download_handle,
                                            const FilePath& path,
                                            const FilePath& original_name);

  // Invoked on the UI thread when a dangerous downloaded file has been renamed.
  void DangerousDownloadRenamed(int64 download_handle,
                                bool success,
                                const FilePath& new_path,
                                int new_path_uniquifier);

  // Updates the app icon about the overall download progress.
  // Marked virtual for testing.
  virtual void UpdateAppIcon();

  // Changes the paths and file name of the specified |download|, propagating
  // the change to the history system.
  void RenameDownload(DownloadItem* download, const FilePath& new_path);

  // Makes the ResourceDispatcherHost pause/un-pause a download request.
  // Called on the IO thread.
  void PauseDownloadRequest(ResourceDispatcherHost* rdh,
                            int render_process_id,
                            int request_id,
                            bool pause);

  // Inform observers that the model has changed.
  void NotifyModelChanged();

  DownloadItem* GetDownloadItem(int id);

  // 'downloads_' is map of all downloads in this profile. The key is the handle
  // returned by the history system, which is unique across sessions. This map
  // owns all the DownloadItems once they have been created in the history
  // system.
  //
  // 'in_progress_' is a map of all downloads that are in progress and that have
  // not yet received a valid history handle. The key is the ID assigned by the
  // ResourceDispatcherHost, which is unique for the current session. This map
  // does not own the DownloadItems.
  //
  // 'dangerous_finished_' is a map of dangerous download that have finished
  // but were not yet approved by the user.  Similarly to in_progress_, the key
  // is the ID assigned by the ResourceDispatcherHost and the map does not own
  // the DownloadItems.  It is used on shutdown to delete completed downloads
  // that have not been approved.
  //
  // When a download is created through a user action, the corresponding
  // DownloadItem* is placed in 'in_progress_' and remains there until it has
  // received a valid handle from the history system. Once it has a valid
  // handle, the DownloadItem* is placed in the 'downloads_' map. When the
  // download is complete, it is removed from 'in_progress_'. Downloads from
  // past sessions read from a persisted state from the history system are
  // placed directly into 'downloads_' since they have valid handles in the
  // history system.
  typedef base::hash_map<int64, DownloadItem*> DownloadMap;
  DownloadMap downloads_;
  DownloadMap in_progress_;
  DownloadMap dangerous_finished_;

  // Collection of all save-page-as downloads in this profile.
  // It owns the DownloadItems.
  std::vector<DownloadItem*> save_page_downloads_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active profile.
  Profile* profile_;
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  scoped_ptr<DownloadHistory> download_history_;

  scoped_ptr<DownloadPrefs> download_prefs_;

  // Non-owning pointer for handling file writing on the download_thread_.
  DownloadFileManager* file_manager_;

  // The user's last choice for download directory. This is only used when the
  // user wants us to prompt for a save location for each download.
  FilePath last_download_path_;

  // Keep track of downloads that are completed before the user selects the
  // destination, so that observers are appropriately notified of completion
  // after this determination is made.
  // The map is of download_id->remaining size (bytes), both of which are
  // required when calling DownloadFinished.
  typedef std::map<int32, int64> PendingFinishedMap;
  PendingFinishedMap pending_finished_downloads_;

  // The "Save As" dialog box used to ask the user where a file should be
  // saved.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  scoped_ptr<OtherDownloadManagerObserver> other_download_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
