// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/filename_util.h"

namespace offline_pages {
namespace {
const base::FilePath::CharType kMHTMLExtension[] = FILE_PATH_LITERAL("mhtml");

void DeleteFileOnFileThread(const base::FilePath& file_path,
                            const base::Closure& callback) {
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path,
                     false /* recursive */),
      callback);
}

// Compute a SHA256 digest using a background thread. The computed digest will
// be returned in the callback parameter. If it is empty, the digest calculation
// fails.
void ComputeDigestOnFileThread(
    const base::FilePath& file_path,
    const base::Callback<void(const std::string&)>& callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&OfflinePageUtils::ComputeDigest, file_path), callback);
}
}  // namespace

// static
OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      weak_ptr_factory_(this) {
  DCHECK(web_contents_);
}

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver()
    : web_contents_(nullptr),
      weak_ptr_factory_(this) {
}

OfflinePageMHTMLArchiver::~OfflinePageMHTMLArchiver() {
}

void OfflinePageMHTMLArchiver::CreateArchive(
    const base::FilePath& archives_dir,
    const CreateArchiveParams& create_archive_params,
    const CreateArchiveCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  callback_ = callback;

  // TODO(chili): crbug/710248 These checks should probably be done inside
  // the offliner.
  if (HasConnectionSecurityError()) {
    ReportFailure(ArchiverResult::ERROR_SECURITY_CERTIFICATE);
    return;
  }

  // Don't save chrome error pages.
  if (GetPageType() == content::PageType::PAGE_TYPE_ERROR) {
    ReportFailure(ArchiverResult::ERROR_ERROR_PAGE);
    return;
  }

  // Don't save chrome-injected interstitial info pages
  // i.e. "This site may be dangerous. Are you sure you want to continue?"
  if (GetPageType() == content::PageType::PAGE_TYPE_INTERSTITIAL) {
    ReportFailure(ArchiverResult::ERROR_INTERSTITIAL_PAGE);
    return;
  }

  GenerateMHTML(archives_dir, create_archive_params);
}

void OfflinePageMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir,
    const CreateArchiveParams& create_archive_params) {
  if (archives_dir.empty()) {
    DVLOG(1) << "Archive path was empty. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  if (!web_contents_) {
    DVLOG(1) << "WebContents is missing. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  if (!web_contents_->GetRenderViewHost()) {
    DVLOG(1) << "RenderViewHost is not created yet. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  GURL url(web_contents_->GetLastCommittedURL());
  base::string16 title(web_contents_->GetTitle());
  base::FilePath file_path(
      archives_dir.Append(base::GenerateGUID()).AddExtension(kMHTMLExtension));
  content::MHTMLGenerationParams params(file_path);
  params.use_binary_encoding = true;
  params.remove_popup_overlay = create_archive_params.remove_popup_overlay;
  params.use_page_problem_detectors =
      create_archive_params.use_page_problem_detectors;

  web_contents_->GenerateMHTML(
      params,
      base::Bind(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                 weak_ptr_factory_.GetWeakPtr(), url, file_path, title));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(
    const GURL& url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size) {
  if (file_size < 0) {
    DeleteFileAndReportFailure(file_path,
                               ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }
  // TODO(jianli): if the file is not in internal directory, there may be a
  // chance that the file gets modified before it has digest computed. So when
  // we support archiving to public directory, we should first put it in
  // internal directory, compute digest, and then move to public directory.
  ComputeDigestOnFileThread(
      file_path, base::Bind(&OfflinePageMHTMLArchiver::OnComputeDigestDone,
                            weak_ptr_factory_.GetWeakPtr(), url, file_path,
                            title, file_size));
}

void OfflinePageMHTMLArchiver::OnComputeDigestDone(
    const GURL& url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size,
    const std::string& digest) {
  if (digest.empty()) {
    DeleteFileAndReportFailure(file_path,
                               ArchiverResult::ERROR_DIGEST_CALCULATION_FAILED);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback_, this, ArchiverResult::SUCCESSFULLY_CREATED, url,
                 file_path, title, file_size, digest));
}

bool OfflinePageMHTMLArchiver::HasConnectionSecurityError() {
  SecurityStateTabHelper::CreateForWebContents(web_contents_);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  return security_state::SecurityLevel::DANGEROUS ==
         security_info.security_level;
}

content::PageType OfflinePageMHTMLArchiver::GetPageType() {
  return web_contents_->GetController().GetVisibleEntry()->GetPageType();
}

void OfflinePageMHTMLArchiver::DeleteFileAndReportFailure(
    const base::FilePath& file_path,
    ArchiverResult result) {
  DeleteFileOnFileThread(file_path,
                         base::Bind(&OfflinePageMHTMLArchiver::ReportFailure,
                                    weak_ptr_factory_.GetWeakPtr(), result));
}

void OfflinePageMHTMLArchiver::ReportFailure(ArchiverResult result) {
  DCHECK(result != ArchiverResult::SUCCESSFULLY_CREATED);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback_, this, result, GURL(), base::FilePath(),
                            base::string16(), 0, std::string()));
}

}  // namespace offline_pages
