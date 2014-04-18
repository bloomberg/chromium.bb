// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/backend_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {

BackendDelegate::BackendDelegate()
    : async_file_util_(new internal::ProviderAsyncFileUtil) {}

BackendDelegate::~BackendDelegate() {}

fileapi::AsyncFileUtil* BackendDelegate::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeProvided, type);
  return async_file_util_.get();
}

scoped_ptr<webkit_blob::FileStreamReader>
BackendDelegate::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeProvided, url.type());
  NOTIMPLEMENTED();
  return scoped_ptr<webkit_blob::FileStreamReader>();
}

scoped_ptr<fileapi::FileStreamWriter> BackendDelegate::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeProvided, url.type());
  NOTIMPLEMENTED();
  return scoped_ptr<fileapi::FileStreamWriter>();
}

}  // namespace file_system_provider
}  // namespace chromeos
