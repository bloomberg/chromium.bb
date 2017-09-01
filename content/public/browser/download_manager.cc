// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager.h"

#include "content/browser/byte_stream.h"
#include "content/browser/download/download_task_runner.h"

namespace content {

// static
scoped_refptr<base::SequencedTaskRunner> DownloadManager::GetTaskRunner() {
  return GetDownloadTaskRunner();
}

DownloadManager::InputStream::InputStream(
    std::unique_ptr<ByteStreamReader> stream_reader)
    : stream_reader_(std::move(stream_reader)) {}

DownloadManager::InputStream::InputStream(
    mojom::DownloadStreamHandlePtr stream_handle)
    : stream_handle_(std::move(stream_handle)) {}

DownloadManager::InputStream::~InputStream() = default;

bool DownloadManager::InputStream::IsEmpty() const {
  return !stream_reader_.get() && stream_handle_.is_null();
}

}  // namespace content
