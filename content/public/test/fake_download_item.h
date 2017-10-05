// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_
#define CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

class FakeDownloadItem : public DownloadItem {
 public:
  FakeDownloadItem();
  ~FakeDownloadItem() override;

  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void NotifyDownloadDestroyed();

  void NotifyDownloadRemoved();

  void NotifyDownloadUpdated();

  void UpdateObservers() override;

  void SetId(uint32_t id);
  uint32_t GetId() const override;

  void SetGuid(const std::string& guid);
  const std::string& GetGuid() const override;

  void SetURL(const GURL& url);
  const GURL& GetURL() const override;

  void SetUrlChain(const std::vector<GURL>& url_chain);
  const std::vector<GURL>& GetUrlChain() const override;

  void SetTargetFilePath(const base::FilePath& file_path);
  const base::FilePath& GetTargetFilePath() const override;

  void SetFileExternallyRemoved(bool is_file_externally_removed);
  bool GetFileExternallyRemoved() const override;

  void SetStartTime(base::Time start_time);
  base::Time GetStartTime() const override;

  void SetEndTime(base::Time end_time);
  base::Time GetEndTime() const override;

  void SetState(const DownloadState& state);
  DownloadState GetState() const override;

  void SetResponseHeaders(
      scoped_refptr<const net::HttpResponseHeaders> response_headers);
  const scoped_refptr<const net::HttpResponseHeaders>& GetResponseHeaders()
      const override;

  void SetMimeType(const std::string& mime_type);
  std::string GetMimeType() const override;

  void SetOriginalUrl(const GURL& url);
  const GURL& GetOriginalUrl() const override;

  void SetLastReason(DownloadInterruptReason last_reason);
  DownloadInterruptReason GetLastReason() const override;

  void SetReceivedBytes(int64_t received_bytes);
  int64_t GetReceivedBytes() const override;

  void SetTotalBytes(int64_t total_bytes);
  int64_t GetTotalBytes() const override;

  void SetLastAccessTime(base::Time time) override;
  base::Time GetLastAccessTime() const override;

  void SetIsTransient(bool is_transient);
  bool IsTransient() const override;

  void SetIsDone(bool is_done);
  bool IsDone() const override;

  void SetETag(const std::string& etag);
  const std::string& GetETag() const override;

  void SetLastModifiedTime(const std::string& last_modified_time);
  const std::string& GetLastModifiedTime() const override;

  // The methods below are not supported and are not expected to be called.
  void ValidateDangerousDownload() override;
  void StealDangerousDownload(bool delete_file_afterward,
                              const AcquireFileCallback& callback) override;
  void Pause() override;
  void Resume() override;
  void Cancel(bool user_cancel) override;
  void Remove() override;
  void OpenDownload() override;
  void ShowDownloadInShell() override;
  bool IsPaused() const override;
  bool IsTemporary() const override;
  bool CanResume() const override;
  const GURL& GetReferrerUrl() const override;
  const GURL& GetSiteUrl() const override;
  const GURL& GetTabUrl() const override;
  const GURL& GetTabReferrerUrl() const override;
  std::string GetSuggestedFilename() const override;
  std::string GetContentDisposition() const override;
  std::string GetOriginalMimeType() const override;
  std::string GetRemoteAddress() const override;
  bool HasUserGesture() const override;
  ui::PageTransition GetTransitionType() const override;
  bool IsSavePackageDownload() const override;
  const base::FilePath& GetFullPath() const override;
  const base::FilePath& GetForcedFilePath() const override;
  base::FilePath GetFileNameToReportUser() const override;
  TargetDisposition GetTargetDisposition() const override;
  const std::string& GetHash() const override;
  void DeleteFile(const base::Callback<void(bool)>& callback) override;
  bool IsDangerous() const override;
  DownloadDangerType GetDangerType() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  int64_t CurrentSpeed() const override;
  int PercentComplete() const override;
  bool AllDataSaved() const override;
  const std::vector<DownloadItem::ReceivedSlice>& GetReceivedSlices()
      const override;
  bool CanShowInFolder() override;
  bool CanOpenDownload() override;
  bool ShouldOpenFileBasedOnExtension() override;
  bool GetOpenWhenComplete() const override;
  bool GetAutoOpened() override;
  bool GetOpened() const override;
  BrowserContext* GetBrowserContext() const override;
  WebContents* GetWebContents() const override;
  void OnContentCheckCompleted(DownloadDangerType danger_type,
                               DownloadInterruptReason reason) override;
  void SetOpenWhenComplete(bool open) override;
  void SetOpened(bool opened) override;
  void SetDisplayName(const base::FilePath& name) override;
  std::string DebugString(bool verbose) const override;

 private:
  base::ObserverList<Observer> observers_;
  uint32_t id_ = 0;
  std::string guid_;
  GURL url_;
  std::vector<GURL> url_chain_;
  base::FilePath file_path_;
  bool is_file_externally_removed_ = false;
  base::Time start_time_;
  base::Time end_time_;
  base::Time last_access_time_;
  // MAX_DOWNLOAD_STATE is used as the uninitialized state.
  DownloadState download_state_ =
      DownloadItem::DownloadState::MAX_DOWNLOAD_STATE;
  scoped_refptr<const net::HttpResponseHeaders> response_headers_;
  std::string mime_type_;
  GURL original_url_;
  DownloadInterruptReason last_reason_ =
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
  int64_t received_bytes_ = 0;
  int64_t total_bytes_ = 0;
  bool is_transient_ = false;
  bool is_done_ = false;
  std::string etag_;
  std::string last_modified_time_;

  // The members below are to be returned by methods, which return by reference.
  std::string dummy_string;
  GURL dummy_url;
  base::FilePath dummy_file_path;

  DISALLOW_COPY_AND_ASSIGN(FakeDownloadItem);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_
