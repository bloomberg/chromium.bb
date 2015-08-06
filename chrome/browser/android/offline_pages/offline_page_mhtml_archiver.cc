// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/offline_pages/offline_page_web_contents_observer.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"

namespace offline_pages {
namespace {
const base::FilePath::CharType kMHTMLExtension[] = FILE_PATH_LITERAL("mhtml");
const base::FilePath::CharType kDefaultFileName[] =
    FILE_PATH_LITERAL("offline_page");

base::FilePath GenerateFileName(GURL url, base::string16 title) {
  return net::GenerateFileName(url,
                               std::string(),             // content disposition
                               std::string(),             // charset
                               base::UTF16ToUTF8(title),  // suggested name
                               std::string(),             // mime-type
                               kDefaultFileName)
      .AddExtension(kMHTMLExtension);
}

}  // namespace

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    content::WebContents* web_contents,
    const base::FilePath& archive_dir)
    : archive_dir_(archive_dir),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  DCHECK(web_contents_);
}

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    const base::FilePath& archive_dir)
    : archive_dir_(archive_dir),
      web_contents_(nullptr),
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
  if (archive_dir_.empty()) {
    DVLOG(1) << "Archive path was empty. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

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
  // TODO(fgorski): Figure out if the actual URL can be different at
  // the end of MHTML generation. Perhaps we should pull it out after the MHTML
  // is generated.
  GURL url(web_contents_->GetLastCommittedURL());
  base::string16 title(web_contents_->GetTitle());
  base::FilePath file_path(archive_dir_.Append(GenerateFileName(url, title)));

  web_contents_->GenerateMHTML(
      file_path, base::Bind(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                            weak_ptr_factory_.GetWeakPtr(), url, file_path));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(
    const GURL& url,
    const base::FilePath& file_path,
    int64 file_size) {
  if (file_size < 0) {
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  } else {
    callback_.Run(this, ArchiverResult::SUCCESSFULLY_CREATED, url, file_path,
                  file_size);
  }
}

void OfflinePageMHTMLArchiver::ReportFailure(ArchiverResult result) {
  DCHECK(result != ArchiverResult::SUCCESSFULLY_CREATED);
  callback_.Run(this, result, GURL(), base::FilePath(), 0);
}

}  // namespace offline_pages
