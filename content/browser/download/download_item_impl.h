// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"


// See download_item.h for usage.
class CONTENT_EXPORT DownloadItemImpl : public content::DownloadItem {
 public:
  // Delegate is defined in DownloadItemImpl (rather than DownloadItem)
  // as it's relevant to the class implementation (class methods need to
  // call into it) and doesn't have anything to do with its interface.
  // Despite this, the delegate methods take DownloadItems as arguments
  // (rather than DownloadItemImpls) so that classes that inherit from it
  // can be used with DownloadItem mocks rather than being tied to
  // DownloadItemImpls.
  class CONTENT_EXPORT Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Used for catching use-after-free errors.
    void Attach();
    void Detach();

    // Tests if a file type should be opened automatically.
    virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) = 0;

    // Allows the delegate to override the opening of a download. If it returns
    // true then it's reponsible for opening the item.
    virtual bool ShouldOpenDownload(DownloadItem* download) = 0;

    // Checks whether a downloaded file still exists and updates the
    // file's state if the file is already removed.
    // The check may or may not result in a later asynchronous call
    // to OnDownloadedFileRemoved().
    virtual void CheckForFileRemoval(DownloadItem* download_item) = 0;

    // If all pre-requisites have been met, complete download processing.
    // TODO(rdsmith): Move into DownloadItem.
    virtual void MaybeCompleteDownload(DownloadItem* download) = 0;

    // For contextual issues like language and prefs.
    virtual content::BrowserContext* GetBrowserContext() const = 0;

    // Handle any delegate portions of a state change operation on the
    // DownloadItem.
    virtual void DownloadCancelled(DownloadItem* download) = 0;
    virtual void DownloadCompleted(DownloadItem* download) = 0;
    virtual void DownloadOpened(DownloadItem* download) = 0;
    virtual void DownloadRemoved(DownloadItem* download) = 0;

    // Assert consistent state for delgate object at various transitions.
    virtual void AssertStateConsistent(DownloadItem* download) const = 0;

   private:
    // For "Outlives attached DownloadItemImpl" invariant assertion.
    int count_;
  };

  // Note that it is the responsibility of the caller to ensure that a
  // DownloadItemImpl::Delegate passed to a DownloadItemImpl constructor
  // outlives the DownloadItemImpl.

  // Constructing from persistent store:
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(Delegate* delegate,
                   content::DownloadId download_id,
                   const content::DownloadPersistentStoreInfo& info,
                   const net::BoundNetLog& bound_net_log);

  // Constructing for a regular download.
  // Takes ownership of the object pointed to by |request_handle|.
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(Delegate* delegate,
                   const DownloadCreateInfo& info,
                   DownloadRequestHandleInterface* request_handle,
                   bool is_otr,
                   const net::BoundNetLog& bound_net_log);

  // Constructing for the "Save Page As..." feature:
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(Delegate* delegate,
                   const FilePath& path,
                   const GURL& url,
                   bool is_otr,
                   content::DownloadId download_id,
                   const std::string& mime_type,
                   const net::BoundNetLog& bound_net_log);

  virtual ~DownloadItemImpl();

  // Overridden from DownloadItem.
  virtual void AddObserver(DownloadItem::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(DownloadItem::Observer* observer) OVERRIDE;
  virtual void UpdateObservers() OVERRIDE;
  virtual bool CanShowInFolder() OVERRIDE;
  virtual bool CanOpenDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension() OVERRIDE;
  virtual void OpenDownload() OVERRIDE;
  virtual void ShowDownloadInShell() OVERRIDE;
  virtual void DangerousDownloadValidated() OVERRIDE;
  virtual void UpdateProgress(int64 bytes_so_far,
                              int64 bytes_per_sec,
                              const std::string& hash_state) OVERRIDE;
  virtual void Cancel(bool user_cancel) OVERRIDE;
  virtual void MarkAsComplete() OVERRIDE;
  virtual void DelayedDownloadOpened() OVERRIDE;
  virtual void OnAllDataSaved(
      int64 size, const std::string& final_hash) OVERRIDE;
  virtual void OnDownloadedFileRemoved() OVERRIDE;
  virtual void MaybeCompleteDownload() OVERRIDE;
  virtual void Interrupted(int64 size,
                           const std::string& hash_state,
                           content::DownloadInterruptReason reason) OVERRIDE;
  virtual void Delete(DeleteReason reason) OVERRIDE;
  virtual void Remove() OVERRIDE;
  virtual bool TimeRemaining(base::TimeDelta* remaining) const OVERRIDE;
  virtual int64 CurrentSpeed() const OVERRIDE;
  virtual int PercentComplete() const OVERRIDE;
  virtual void OnPathDetermined(const FilePath& path) OVERRIDE;
  virtual bool AllDataSaved() const OVERRIDE;
  virtual void SetFileCheckResults(const DownloadStateInfo& state) OVERRIDE;
  virtual void Rename(const FilePath& full_path) OVERRIDE;
  virtual void TogglePause() OVERRIDE;
  virtual void OnDownloadCompleting(DownloadFileManager* file_manager) OVERRIDE;
  virtual void OnDownloadRenamedToFinalName(const FilePath& full_path) OVERRIDE;
  virtual bool MatchesQuery(const string16& query) const OVERRIDE;
  virtual bool IsPartialDownload() const OVERRIDE;
  virtual bool IsInProgress() const OVERRIDE;
  virtual bool IsCancelled() const OVERRIDE;
  virtual bool IsInterrupted() const OVERRIDE;
  virtual bool IsComplete() const OVERRIDE;
  virtual DownloadState GetState() const OVERRIDE;
  virtual const FilePath& GetFullPath() const OVERRIDE;
  virtual void SetPathUniquifier(int uniquifier) OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const std::vector<GURL>& GetUrlChain() const OVERRIDE;
  virtual const GURL& GetOriginalUrl() const OVERRIDE;
  virtual const GURL& GetReferrerUrl() const OVERRIDE;
  virtual std::string GetSuggestedFilename() const OVERRIDE;
  virtual std::string GetContentDisposition() const OVERRIDE;
  virtual std::string GetMimeType() const OVERRIDE;
  virtual std::string GetOriginalMimeType() const OVERRIDE;
  virtual std::string GetReferrerCharset() const OVERRIDE;
  virtual std::string GetRemoteAddress() const OVERRIDE;
  virtual int64 GetTotalBytes() const OVERRIDE;
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual const std::string& GetHash() const OVERRIDE;
  virtual int64 GetReceivedBytes() const OVERRIDE;
  virtual const std::string& GetHashState() const OVERRIDE;
  virtual int32 GetId() const OVERRIDE;
  virtual content::DownloadId GetGlobalId() const OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual base::Time GetEndTime() const OVERRIDE;
  virtual void SetIsPersisted() OVERRIDE;
  virtual bool IsPersisted() const OVERRIDE;
  virtual void SetDbHandle(int64 handle) OVERRIDE;
  virtual int64 GetDbHandle() const OVERRIDE;
  virtual bool IsPaused() const OVERRIDE;
  virtual bool GetOpenWhenComplete() const OVERRIDE;
  virtual void SetOpenWhenComplete(bool open) OVERRIDE;
  virtual bool GetFileExternallyRemoved() const OVERRIDE;
  virtual SafetyState GetSafetyState() const OVERRIDE;
  virtual content::DownloadDangerType GetDangerType() const OVERRIDE;
  virtual void SetDangerType(content::DownloadDangerType danger_type) OVERRIDE;
  virtual bool IsDangerous() const OVERRIDE;
  virtual bool GetAutoOpened() OVERRIDE;
  virtual const FilePath& GetTargetName() const OVERRIDE;
  virtual bool PromptUserForSaveLocation() const OVERRIDE;
  virtual bool IsOtr() const OVERRIDE;
  virtual const FilePath& GetSuggestedPath() const OVERRIDE;
  virtual bool IsTemporary() const OVERRIDE;
  virtual void SetIsTemporary(bool temporary) OVERRIDE;
  virtual void SetOpened(bool opened) OVERRIDE;
  virtual bool GetOpened() const OVERRIDE;
  virtual const std::string& GetLastModifiedTime() const OVERRIDE;
  virtual const std::string& GetETag() const OVERRIDE;
  virtual content::DownloadInterruptReason GetLastReason() const OVERRIDE;
  virtual content::DownloadPersistentStoreInfo
      GetPersistentStoreInfo() const OVERRIDE;
  virtual DownloadStateInfo GetStateInfo() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual FilePath GetTargetFilePath() const OVERRIDE;
  virtual FilePath GetFileNameToReportUser() const OVERRIDE;
  virtual void SetDisplayName(const FilePath& name) OVERRIDE;
  virtual FilePath GetUserVerifiedFilePath() const OVERRIDE;
  virtual bool NeedsRename() const OVERRIDE;
  virtual void OffThreadCancel(DownloadFileManager* file_manager) OVERRIDE;
  virtual std::string DebugString(bool verbose) const OVERRIDE;
  virtual void MockDownloadOpenForTesting() OVERRIDE;
  virtual ExternalData* GetExternalData(const void* key) OVERRIDE;
  virtual const ExternalData* GetExternalData(const void* key) const OVERRIDE;
  virtual void SetExternalData(const void* key, ExternalData* data) OVERRIDE;

 private:
  // Construction common to all constructors. |active| should be true for new
  // downloads and false for downloads from the history.
  // |download_type| indicates to the net log system what kind of download
  // this is.
  void Init(bool active, download_net_logs::DownloadType download_type);

  // Internal helper for maintaining consistent received and total sizes, and
  // hash state.
  void UpdateProgress(int64 bytes_so_far, const std::string& hash_state);

  // Internal helper for maintaining consistent received and total sizes, and
  // setting the final hash.
  // Should only be called from |OnAllDataSaved|.
  void ProgressComplete(int64 bytes_so_far,
                        const std::string& final_hash);

  // Called when the entire download operation (including renaming etc)
  // is completed.
  void Completed();

  // Call to transition state; all state transitions should go through this.
  void TransitionTo(DownloadState new_state);

  // Called when safety_state_ should be recomputed from is_dangerous_file
  // and is_dangerous_url.
  void UpdateSafetyState();

  // Helper function to recompute |state_info_.target_name| when
  // it may have changed.  (If it's non-null it should be left alone,
  // otherwise updated from |full_path_|.)
  void UpdateTarget();

  // State information used by the download manager.
  DownloadStateInfo state_info_;

  // Display name for the download if different from the target filename.
  FilePath display_name_;

  // The handle to the request information.  Used for operations outside the
  // download system.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  // Download ID assigned by DownloadResourceHandler.
  content::DownloadId download_id_;

  // Full path to the downloaded or downloading file.
  FilePath full_path_;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

  // Filename suggestion from DownloadSaveInfo. It could, among others, be the
  // suggested filename in 'download' attribute of an anchor. Details:
  // http://www.whatwg.org/specs/web-apps/current-work/#downloading-hyperlinks
  std::string suggested_filename_;

  // Information from the request.
  // Content-disposition field from the header.
  std::string content_disposition_;

  // Mime-type from the header.  Subject to change.
  std::string mime_type_;

  // The value of the content type header sent with the downloaded item.  It
  // may be different from |mime_type_|, which may be set based on heuristics
  // which may look at the file extension and first few bytes of the file.
  std::string original_mime_type_;

  // The charset of the referring page where the download request comes from.
  // It's used to construct a suggested filename.
  std::string referrer_charset_;

  // The remote IP address where the download was fetched from.  Copied from
  // DownloadCreateInfo::remote_address.
  std::string remote_address_;

  // Total bytes expected.
  int64 total_bytes_;

  // Current received bytes.
  int64 received_bytes_;

  // Current speed. Calculated by the DownloadFile.
  int64 bytes_per_sec_;

  // Sha256 hash of the content.  This might be empty either because
  // the download isn't done yet or because the hash isn't needed
  // (ChromeDownloadManagerDelegate::GenerateFileHash() returned false).
  std::string hash_;

  // A blob containing the state of the hash algorithm.  Only valid while the
  // download is in progress.
  std::string hash_state_;

  // Server's time stamp for the file.
  std::string last_modified_time_;

  // Server's ETAG for the file.
  std::string etag_;

  // Last reason.
  content::DownloadInterruptReason last_reason_;

  // Start time for recording statistics.
  base::TimeTicks start_tick_;

  // The current state of this download.
  DownloadState state_;

  // The views of this item in the download shelf and download contents.
  ObserverList<Observer> observers_;

  // Time the download was started.
  base::Time start_time_;

  // Time the download completed.
  base::Time end_time_;

  // Our persistent store handle.
  int64 db_handle_;

  // Our delegate.
  Delegate* delegate_;

  // In progress downloads may be paused by the user, we note it here.
  bool is_paused_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // A flag for indicating if the downloaded file is externally removed.
  bool file_externally_removed_;

  // Indicates if the download is considered potentially safe or dangerous
  // (executable files are typically considered dangerous).
  SafetyState safety_state_;

  // True if the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_;

  bool is_persisted_;

  // True if the download was initiated in an incognito window.
  bool is_otr_;

  // True if the item was downloaded temporarily.
  bool is_temporary_;

  // True if we've saved all the data for the download.
  bool all_data_saved_;

  // Did the user open the item either directly or indirectly (such as by
  // setting always open files of this type)? The shelf also sets this field
  // when the user closes the shelf before the item has been opened but should
  // be treated as though the user opened it.
  bool opened_;

  // Do we actually open downloads when requested?  For testing purposes only.
  bool open_enabled_;

  // Did the delegate delay calling Complete on this download?
  bool delegate_delayed_complete_;

  // External Data storage.  All objects in the store
  // are owned by the DownloadItemImpl.
  std::map<const void*, ExternalData*> external_data_map_;

  // Net log to use for this download.
  const net::BoundNetLog bound_net_log_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
