// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_file_stream_reader.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

ArcDocumentsProviderFileStreamReader::ArcDocumentsProviderFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    ArcDocumentsProviderRootMap* roots)
    : offset_(offset), content_url_resolved_(false), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    content_url_resolved_ = true;
    return;
  }
  root->ResolveToContentUrl(
      path,
      base::Bind(&ArcDocumentsProviderFileStreamReader::OnResolveToContentUrl,
                 weak_ptr_factory_.GetWeakPtr()));
}

ArcDocumentsProviderFileStreamReader::~ArcDocumentsProviderFileStreamReader() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

int ArcDocumentsProviderFileStreamReader::Read(
    net::IOBuffer* buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!content_url_resolved_) {
    pending_operations_.emplace_back(base::Bind(
        &ArcDocumentsProviderFileStreamReader::RunPendingRead,
        base::Unretained(this), base::Passed(make_scoped_refptr(buffer)),
        buffer_length, callback));
    return net::ERR_IO_PENDING;
  }
  if (!underlying_reader_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_reader_->Read(buffer, buffer_length, callback);
}

int64_t ArcDocumentsProviderFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!content_url_resolved_) {
    pending_operations_.emplace_back(
        base::Bind(&ArcDocumentsProviderFileStreamReader::RunPendingGetLength,
                   base::Unretained(this), callback));
    return net::ERR_IO_PENDING;
  }
  if (!underlying_reader_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_reader_->GetLength(callback);
}

void ArcDocumentsProviderFileStreamReader::OnResolveToContentUrl(
    const GURL& content_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!content_url_resolved_);

  if (content_url.is_valid()) {
    underlying_reader_ = base::MakeUnique<ArcContentFileSystemFileStreamReader>(
        content_url, offset_);
  }
  content_url_resolved_ = true;

  std::vector<base::Closure> pending_operations;
  pending_operations.swap(pending_operations_);
  for (const base::Closure& callback : pending_operations) {
    callback.Run();
  }
}

void ArcDocumentsProviderFileStreamReader::RunPendingRead(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  int result =
      underlying_reader_
          ? underlying_reader_->Read(buffer.get(), buffer_length, callback)
          : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void ArcDocumentsProviderFileStreamReader::RunPendingGetLength(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  int64_t result = underlying_reader_ ? underlying_reader_->GetLength(callback)
                                      : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace arc
