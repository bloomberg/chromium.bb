// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"

namespace offline_pages {
namespace {
const base::FilePath::CharType kMHTMLExtension[] = FILE_PATH_LITERAL("mhtml");
const base::FilePath::CharType kDefaultFileName[] =
    FILE_PATH_LITERAL("offline_page");
const int kTitleLengthMax = 80;
const char kMHTMLFileNameExtension[] = ".mhtml";
const char kFileNameComponentsSeparator[] = "-";
const char kReplaceChars[] = " ";
const char kReplaceWith[] = "_";
}  // namespace

// static
std::string OfflinePageMHTMLArchiver::GetFileNameExtension() {
    return kMHTMLFileNameExtension;
}

// static
base::FilePath OfflinePageMHTMLArchiver::GenerateFileName(
    const GURL& url,
    const std::string& title,
    int64_t archive_id) {
  std::string title_part(title.substr(0, kTitleLengthMax));
  std::string suggested_name(
      url.host() + kFileNameComponentsSeparator +
      title_part + kFileNameComponentsSeparator +
      base::Int64ToString(archive_id));

  // Substitute spaces out from title.
  base::ReplaceChars(suggested_name, kReplaceChars, kReplaceWith,
                     &suggested_name);

  return net::GenerateFileName(url,
                               std::string(),  // content disposition
                               std::string(),  // charset
                               suggested_name,
                               std::string(),  // mime-type
                               kDefaultFileName)
      .AddExtension(kMHTMLExtension);
}

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
    int64_t archive_id,
    const CreateArchiveCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  callback_ = callback;

  GenerateMHTML(archives_dir, archive_id);
}

void OfflinePageMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir, int64_t archive_id) {
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

  // TODO(fgorski): Figure out if the actual URL can be different at
  // the end of MHTML generation. Perhaps we should pull it out after the MHTML
  // is generated.
  GURL url(web_contents_->GetLastCommittedURL());
  base::string16 title(web_contents_->GetTitle());
  base::FilePath file_path(
      archives_dir.Append(
          GenerateFileName(url, base::UTF16ToUTF8(title), archive_id)));

  web_contents_->GenerateMHTML(
      file_path, true /* use_binary_encoding */,
      base::Bind(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                 weak_ptr_factory_.GetWeakPtr(), url, file_path));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(
    const GURL& url,
    const base::FilePath& file_path,
    int64_t file_size) {
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
