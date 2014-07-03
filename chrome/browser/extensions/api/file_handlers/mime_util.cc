// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/mime_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "webkit/browser/fileapi/file_system_url.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#endif

using content::BrowserThread;

namespace extensions {
namespace app_file_handler_util {
namespace {

// Detects MIME type by reading initial bytes from the file. If found, then
// writes the MIME type to |result|.
void SniffMimeType(const base::FilePath& local_path, std::string* result) {
  std::vector<char> content(net::kMaxBytesToSniff);

  const int bytes_read =
      base::ReadFile(local_path, &content[0], content.size());

  if (bytes_read >= 0) {
    net::SniffMimeType(&content[0],
                       bytes_read,
                       net::FilePathToFileURL(local_path),
                       std::string(),  // type_hint (passes no hint)
                       result);
  }
}

#if defined(OS_CHROMEOS)
// Called when fetching MIME type for a non-native local path is completed.
// If |success| is false, then tries to guess the MIME type by looking at the
// file name.
void OnGetMimeTypeForNonNativeLocalPathCompleted(
    const base::FilePath& local_path,
    const base::Callback<void(const std::string&)>& callback,
    bool success,
    const std::string& mime_type) {
  if (success) {
    callback.Run(mime_type);
    return;
  }

  std::string mime_type_from_extension;
  net::GetMimeTypeFromFile(local_path, &mime_type_from_extension);
  callback.Run(mime_type_from_extension);
}
#endif

// Called when sniffing for MIME type in the native local file is completed.
void OnSniffMimeTypeForNativeLocalPathCompleted(
    scoped_ptr<std::string> mime_type,
    const base::Callback<void(const std::string&)>& callback) {
  callback.Run(*mime_type);
}

}  // namespace

void GetMimeTypeForLocalPath(
    Profile* profile,
    const base::FilePath& local_path,
    const base::Callback<void(const std::string&)>& callback) {
#if defined(OS_CHROMEOS)
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, local_path)) {
    // For non-native files, try to get the MIME type from metadata. If not
    // available, then try to guess from the extension. Never sniff (because
    // it can be very slow).
    file_manager::util::GetNonNativeLocalPathMimeType(
        profile,
        local_path,
        base::Bind(&OnGetMimeTypeForNonNativeLocalPathCompleted,
                   local_path,
                   callback));
    return;
  }
#endif

  // For native local files, try to guess the mime from the extension. If
  // not availble, then try to sniff if.
  std::string mime_type_from_extension;
  if (net::GetMimeTypeFromFile(local_path, &mime_type_from_extension)) {
    callback.Run(mime_type_from_extension);
  } else {
    scoped_ptr<std::string> sniffed_mime_type(new std::string);
    std::string* sniffed_mime_type_ptr = sniffed_mime_type.get();
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&SniffMimeType, local_path, sniffed_mime_type_ptr),
        base::Bind(&OnSniffMimeTypeForNativeLocalPathCompleted,
                   base::Passed(&sniffed_mime_type),
                   callback));
  }
}

MimeTypeCollector::MimeTypeCollector(Profile* profile)
    : profile_(profile), left_(0), weak_ptr_factory_(this) {
}

MimeTypeCollector::~MimeTypeCollector() {
}

void MimeTypeCollector::CollectForURLs(
    const std::vector<fileapi::FileSystemURL>& urls,
    const CompletionCallback& callback) {
  std::vector<base::FilePath> local_paths;
  for (size_t i = 0; i < urls.size(); ++i) {
    local_paths.push_back(urls[i].path());
  }

  CollectForLocalPaths(local_paths, callback);
}

void MimeTypeCollector::CollectForLocalPaths(
    const std::vector<base::FilePath>& local_paths,
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  callback_ = callback;

  DCHECK(!result_.get());
  result_.reset(new std::vector<std::string>(local_paths.size()));
  left_ = local_paths.size();

  if (!left_) {
    // Nothing to process.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback_, base::Passed(&result_)));
    callback_ = CompletionCallback();
    return;
  }

  for (size_t i = 0; i < local_paths.size(); ++i) {
    GetMimeTypeForLocalPath(profile_,
                            local_paths[i],
                            base::Bind(&MimeTypeCollector::OnMimeTypeCollected,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       i));
  }
}

void MimeTypeCollector::OnMimeTypeCollected(size_t index,
                                            const std::string& mime_type) {
  (*result_)[index] = mime_type;
  if (!--left_) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback_, base::Passed(&result_)));
    // Release the callback to avoid a circullar reference in case an instance
    // of this class is a member of a ref counted class, which instance is bound
    // to this callback.
    callback_ = CompletionCallback();
  }
}

}  // namespace app_file_handler_util
}  // namespace extensions
