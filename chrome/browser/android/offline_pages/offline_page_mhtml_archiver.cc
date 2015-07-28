// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/offline_pages/offline_page_web_contents_observer.h"
#include "content/public/browser/web_contents.h"

namespace offline_pages {

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    content::WebContents* web_contents,
    const base::FilePath& file_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : file_path_(file_path),
      web_contents_(web_contents),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
  DCHECK(web_contents_);
}

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    const base::FilePath& file_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : file_path_(file_path),
      web_contents_(nullptr),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
}

OfflinePageMHTMLArchiver::~OfflinePageMHTMLArchiver() {
  if (web_contents_) {
    OfflinePageWebContentsObserver* web_contents_observer =
        OfflinePageWebContentsObserver::FromWebContents(web_contents_);
    if (web_contents_observer)
      web_contents_observer->set_main_frame_document_loaded_callback(
          base::Closure());
  }
}

void OfflinePageMHTMLArchiver::CreateArchive(
    const CreateArchiveCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  callback_ = callback;

  GenerateMHTML();
}

void OfflinePageMHTMLArchiver::GenerateMHTML() {
  if (!web_contents_) {
    DVLOG(1) << "WebContents is missing. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  OfflinePageWebContentsObserver* web_contents_observer =
      OfflinePageWebContentsObserver::FromWebContents(web_contents_);
  if (!web_contents_observer) {
    DVLOG(1) << "WebContentsObserver is missing. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  // If main frame document has not been loaded yet, wait until it is.
  if (!web_contents_observer->is_document_loaded_in_main_frame()) {
    web_contents_observer->set_main_frame_document_loaded_callback(
        base::Bind(&OfflinePageMHTMLArchiver::DoGenerateMHTML,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DoGenerateMHTML();
}

void OfflinePageMHTMLArchiver::DoGenerateMHTML() {
  // TODO(fgorski): Figure out if the actual URL or title can be different at
  // the end of MHTML generation. Perhaps we should pull it out after the MHTML
  // is generated.
  web_contents_->GenerateMHTML(
      file_path_, base::Bind(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                             weak_ptr_factory_.GetWeakPtr(),
                             web_contents_->GetLastCommittedURL(),
                             web_contents_->GetTitle()));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(const GURL& url,
                                                   const base::string16& title,
                                                   int64 file_size) {
  ArchiverResult result =
      file_size < 0 ? ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED :
                      ArchiverResult::SUCCESSFULLY_CREATED;
  ReportResult(result, url, title, file_size);
}

void OfflinePageMHTMLArchiver::ReportFailure(ArchiverResult result) {
  DCHECK(result != ArchiverResult::SUCCESSFULLY_CREATED);
  ReportResult(result, GURL(), base::string16(), 0);
}

void OfflinePageMHTMLArchiver::ReportResult(ArchiverResult result,
                                            const GURL& url,
                                            const base::string16& title,
                                            int64 file_size) {
  base::FilePath file_path;
  if (result == ArchiverResult::SUCCESSFULLY_CREATED) {
    // Pass an actual file path and report file size only upon success.
    file_path = file_path_;
  } else {
    // Make sure both file path and file size are empty on failure.
    file_size = 0;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(
      callback_, this, result, url, title, file_path, file_size));
}

}  // namespace offline_pages
