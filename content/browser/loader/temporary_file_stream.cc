// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/temporary_file_stream.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "webkit/common/blob/shareable_file_reference.h"

using webkit_blob::ShareableFileReference;

namespace content {

namespace {

void DidCreateTemporaryFile(
    const CreateTemporaryFileStreamCallback& callback,
    base::File::Error error_code,
    base::PassPlatformFile file_handle,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error_code != base::File::FILE_OK) {
    callback.Run(error_code, scoped_ptr<net::FileStream>(), NULL);
    return;
  }

  // Cancelled or not, create the deletable_file so the temporary is cleaned up.
  scoped_refptr<ShareableFileReference> deletable_file =
      ShareableFileReference::GetOrCreate(
          file_path,
          ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          BrowserThread::GetMessageLoopProxyForThread(
              BrowserThread::FILE).get());

  scoped_ptr<net::FileStream> file_stream(new net::FileStream(
      file_handle.ReleaseValue(),
      base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC,
      NULL));

  callback.Run(error_code, file_stream.Pass(), deletable_file);
}

}  // namespace

void CreateTemporaryFileStream(
    const CreateTemporaryFileStreamCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FileUtilProxy::CreateTemporary(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE).get(),
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&DidCreateTemporaryFile, callback));
}

}  // namespace content
