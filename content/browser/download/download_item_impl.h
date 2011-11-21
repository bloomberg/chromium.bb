// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_state_info.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

class DownloadFileManager;
class DownloadId;
class DownloadManager;
class TabContents;

struct DownloadCreateInfo;
struct DownloadPersistentStoreInfo;

// See download_item.h for usage.
class CONTENT_EXPORT DownloadItemImpl : public DownloadItem {
 public:
  // Constructing from persistent store:
  DownloadItemImpl(DownloadManager* download_manager,
                   const DownloadPersistentStoreInfo& info);

  // Constructing for a regular download.
  // Takes ownership of the object pointed to by |request_handle|.
  DownloadItemImpl(DownloadManager* download_manager,
                   const DownloadCreateInfo& info,
                   DownloadRequestHandleInterface* request_handle,
                   bool is_otr);

  // Constructing for the "Save Page As..." feature:
  DownloadItemImpl(DownloadManager* download_manager,
                   const FilePath& path,
                   const GURL& url,
                   bool is_otr,
                   DownloadId download_id);

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
  virtual void Update(int64 bytes_so_far) OVERRIDE;
  virtual void Cancel(bool user_cancel) OVERRIDE;
  virtual void MarkAsComplete() OVERRIDE;
  virtual void DelayedDownloadOpened() OVERRIDE;
  virtual void OnAllDataSaved(
      int64 size, const std::string& final_hash) OVERRIDE;
  virtual void OnDownloadedFileRemoved() OVERRIDE;
  virtual void Interrupted(int64 size, InterruptReason reason) OVERRIDE;
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
  virtual int64 GetTotalBytes() const OVERRIDE;
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual const std::string& GetHash() const OVERRIDE;
  virtual int64 GetReceivedBytes() const OVERRIDE;
  virtual int32 GetId() const OVERRIDE;
  virtual DownloadId GetGlobalId() const OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual base::Time GetEndTime() const OVERRIDE;
  virtual void SetDbHandle(int64 handle) OVERRIDE;
  virtual int64 GetDbHandle() const OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual bool IsPaused() const OVERRIDE;
  virtual bool GetOpenWhenComplete() const OVERRIDE;
  virtual void SetOpenWhenComplete(bool open) OVERRIDE;
  virtual bool GetFileExternallyRemoved() const OVERRIDE;
  virtual SafetyState GetSafetyState() const OVERRIDE;
  virtual DownloadStateInfo::DangerType GetDangerType() const OVERRIDE;
  virtual bool IsDangerous() const OVERRIDE;
  virtual void MarkFileDangerous() OVERRIDE;
  virtual void MarkUrlDangerous() OVERRIDE;
  virtual void MarkContentDangerous() OVERRIDE;
  virtual bool GetAutoOpened() OVERRIDE;
  virtual const FilePath& GetTargetName() const OVERRIDE;
  virtual bool PromptUserForSaveLocation() const OVERRIDE;
  virtual bool IsOtr() const OVERRIDE;
  virtual const FilePath& GetSuggestedPath() const OVERRIDE;
  virtual bool IsTemporary() const OVERRIDE;
  virtual void SetOpened(bool opened) OVERRIDE;
  virtual bool GetOpened() const OVERRIDE;
  virtual InterruptReason GetLastReason() const OVERRIDE;
  virtual DownloadPersistentStoreInfo GetPersistentStoreInfo() const OVERRIDE;
  virtual DownloadStateInfo GetStateInfo() const OVERRIDE;
  virtual TabContents* GetTabContents() const OVERRIDE;
  virtual FilePath GetTargetFilePath() const OVERRIDE;
  virtual FilePath GetFileNameToReportUser() const OVERRIDE;
  virtual FilePath GetUserVerifiedFilePath() const OVERRIDE;
  virtual bool NeedsRename() const OVERRIDE;
  virtual void OffThreadCancel(DownloadFileManager* file_manager) OVERRIDE;
  virtual std::string DebugString(bool verbose) const OVERRIDE;
  virtual void MockDownloadOpenForTesting() OVERRIDE;

 private:
  // Construction common to all constructors. |active| should be true for new
  // downloads and false for downloads from the history.
  void Init(bool active);

  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

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

  // The handle to the request information.  Used for operations outside the
  // download system.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  // Download ID assigned by DownloadResourceHandler.
  DownloadId download_id_;

  // Full path to the downloaded or downloading file.
  FilePath full_path_;

  // A number that should be appended to the path to make it unique, or 0 if the
  // path should be used as is.
  int path_uniquifier_;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

  // Suggested filename in 'download' attribute of an anchor. Details:
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

  // Total bytes expected
  int64 total_bytes_;

  // Current received bytes
  int64 received_bytes_;

  // Sha256 hash of the content.  This might be empty either because
  // the download isn't done yet or because the hash isn't needed
  // (ChromeDownloadManagerDelegate::GenerateFileHash() returned false).
  std::string hash_;

  // Last reason.
  InterruptReason last_reason_;

  // Start time for calculating remaining time
  base::TimeTicks start_tick_;

  // The current state of this download
  DownloadState state_;

  // The views of this item in the download shelf and download tab
  ObserverList<Observer> observers_;

  // Time the download was started
  base::Time start_time_;

  // Time the download completed
  base::Time end_time_;

  // Our persistent store handle
  int64 db_handle_;

  // Our owning object
  DownloadManager* download_manager_;

  // In progress downloads may be paused by the user, we note it here
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

  // Do we actual open downloads when requested?  For testing purposes
  // only.
  bool open_enabled_;

  // Did the delegate delay calling Complete on this download?
  bool delegate_delayed_complete_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
