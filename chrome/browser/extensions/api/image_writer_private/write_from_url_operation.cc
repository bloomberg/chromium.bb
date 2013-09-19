// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromUrlOperation::WriteFromUrlOperation(
    OperationManager* manager,
    const ExtensionId& extension_id,
    content::RenderViewHost* rvh,
    GURL url,
    const std::string& hash,
    bool saveImageAsDownload,
    const std::string& storage_unit_id)
    : Operation(manager, extension_id, storage_unit_id),
      rvh_(rvh),
      url_(url),
      hash_(hash),
      saveImageAsDownload_(saveImageAsDownload),
      download_(NULL){
}

WriteFromUrlOperation::~WriteFromUrlOperation() {
  if (stage_ == image_writer_api::STAGE_DOWNLOAD && download_) {
    download_->RemoveObserver(this);
    download_->Cancel(false);
  }
}
void WriteFromUrlOperation::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  stage_ = image_writer_api::STAGE_DOWNLOAD;
  progress_ = 0;

  if (saveImageAsDownload_){
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WriteFromUrlOperation::DownloadStart, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WriteFromUrlOperation::CreateTempFile, this));
  }
}

void WriteFromUrlOperation::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (stage_ == image_writer_api::STAGE_DOWNLOAD && download_) {
    download_->RemoveObserver(this);
    download_->Cancel(true);
  }

  Operation::Cancel();
}

void WriteFromUrlOperation::CreateTempFile() {
  if (IsCancelled()) {
    return;
  }

  tmp_file_.reset(new base::FilePath());

  if (file_util::CreateTemporaryFile(tmp_file_.get())) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WriteFromUrlOperation::DownloadStart, this));
  } else {
    Error(error::kTempFile);
  }
}

// The downloader runs on the UI thread.
void WriteFromUrlOperation::DownloadStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Starting download of URL: " << url_;

  if (IsCancelled()) {
    return;
  }

  stage_ = image_writer_api::STAGE_DOWNLOAD;
  progress_ = 0;

  Profile* current_profile = manager_->profile();

  scoped_ptr<content::DownloadUrlParameters> download_params(
      new content::DownloadUrlParameters(
        url_,
        rvh_->GetProcess()->GetID(),
        rvh_->GetRoutingID(),
        current_profile->GetResourceContext()));

  if (tmp_file_.get()) {
    download_params->set_file_path(*tmp_file_);
  }

  download_params->set_callback(
      base::Bind(&WriteFromUrlOperation::OnDownloadStarted, this));

  content::DownloadManager* download_manager =
    content::BrowserContext::GetDownloadManager(current_profile);
  download_manager->DownloadUrl(download_params.Pass());
}

void WriteFromUrlOperation::OnDownloadStarted(content::DownloadItem* item,
                                              net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (item) {
    DCHECK_EQ(net::OK, error);

    download_ = item;
    download_->AddObserver(this);

    // Run at least once.
    OnDownloadUpdated(download_);
  } else {
    DCHECK_NE(net::OK, error);
    std::string error_message = ErrorUtils::FormatErrorMessage(
        "Download failed: *",
        net::ErrorToString(error));
    Error(error_message);
  }
}

// Always called from the UI thread.
void WriteFromUrlOperation::OnDownloadUpdated(
  content::DownloadItem* download) {

  if (IsCancelled()) {
    download->Cancel(false);
    return;
  }

  progress_ = download->PercentComplete();

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WriteFromUrlOperation::SendProgress, this));

  if (download->GetState() == content::DownloadItem::COMPLETE) {
    download_path_ = download_->GetTargetFilePath();

    download_->RemoveObserver(this);
    download_ = NULL;

    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WriteFromUrlOperation::DownloadComplete, this));

  } else if (download->GetState() == content::DownloadItem::INTERRUPTED) {
    Error(error::kDownloadInterrupted);
  } else if (download->GetState() == content::DownloadItem::CANCELLED) {
    Error(error::kDownloadCancelled);
  }
}

void WriteFromUrlOperation::DownloadComplete() {
  DVLOG(1) << "Download complete.";

  progress_ = 100;
  SendProgress();

  VerifyDownloadStart();
}

void WriteFromUrlOperation::VerifyDownloadStart() {
  if (IsCancelled()) {
    return;
  }

  // Skip verify if no hash.
  if (hash_.empty()) {
    scoped_ptr<base::FilePath> download_path(
        new base::FilePath(download_path_));
    UnzipStart(download_path.Pass());
    return;
  }

  DVLOG(1) << "Download verification started.";

  stage_ = image_writer_api::STAGE_VERIFYDOWNLOAD;
  progress_ = 0;

  SendProgress();

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WriteFromUrlOperation::VerifyDownloadRun, this));
}

void WriteFromUrlOperation::VerifyDownloadRun() {
  scoped_ptr<base::FilePath> download_path(new base::FilePath(download_path_));
  GetMD5SumOfFile(
      download_path.Pass(),
      0,
      0,
      100,
      base::Bind(&WriteFromUrlOperation::VerifyDownloadCompare, this));
}

void WriteFromUrlOperation::VerifyDownloadCompare(
    scoped_ptr<std::string> download_hash) {
  if (*download_hash != hash_) {
    Error(error::kDownloadHash);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WriteFromUrlOperation::VerifyDownloadComplete, this));
}

void WriteFromUrlOperation::VerifyDownloadComplete() {
  if (IsCancelled()) {
    return;
  }

  DVLOG(1) << "Download verification complete.";

  progress_ = 100;
  SendProgress();

  scoped_ptr<base::FilePath> download_path(new base::FilePath(download_path_));
  UnzipStart(download_path.Pass());
}

} // namespace image_writer
} // namespace extensions
