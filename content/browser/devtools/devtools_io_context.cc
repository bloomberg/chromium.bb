// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_io_context.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/devtools/devtools_stream_blob.h"
#include "content/browser/devtools/devtools_stream_file.h"

namespace content {

DevToolsIOContext::ROStream::ROStream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RefCountedDeleteOnSequence<DevToolsIOContext::ROStream>(
          std::move(task_runner)) {}

DevToolsIOContext::ROStream::~ROStream() = default;

DevToolsIOContext::RWStream::RWStream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : DevToolsIOContext::ROStream(std::move(task_runner)) {}

DevToolsIOContext::RWStream::~RWStream() = default;

DevToolsIOContext::DevToolsIOContext() : weak_factory_(this) {}

DevToolsIOContext::~DevToolsIOContext() {
  DiscardAllStreams();
}

scoped_refptr<DevToolsIOContext::RWStream>
DevToolsIOContext::CreateTempFileBackedStream(bool binary) {
  scoped_refptr<DevToolsStreamFile> result = new DevToolsStreamFile(binary);
  bool inserted =
      streams_.insert(std::make_pair(result->handle(), result)).second;
  DCHECK(inserted);
  return result;
}

scoped_refptr<DevToolsIOContext::ROStream> DevToolsIOContext::GetByHandle(
    const std::string& handle) {
  StreamsMap::const_iterator it = streams_.find(handle);
  return it == streams_.end() ? scoped_refptr<ROStream>() : it->second;
}

scoped_refptr<DevToolsIOContext::ROStream> DevToolsIOContext::OpenBlob(
    ChromeBlobStorageContext* context,
    StoragePartition* partition,
    const std::string& handle,
    const std::string& uuid) {
  scoped_refptr<DevToolsStreamBlob> result = new DevToolsStreamBlob();
  bool inserted = streams_.insert(std::make_pair(handle, result)).second;

  result->Open(context, partition, uuid,
               base::BindOnce(&DevToolsIOContext::OnBlobOpenComplete,
                              weak_factory_.GetWeakPtr(), handle));
  DCHECK(inserted);
  return std::move(result);
}

void DevToolsIOContext::OnBlobOpenComplete(const std::string& handle,
                                           bool success) {
  if (!success)
    Close(handle);
}

bool DevToolsIOContext::Close(const std::string& handle) {
  StreamsMap::iterator it = streams_.find(handle);
  if (it == streams_.end())
    return false;
  it->second->Close(false);
  streams_.erase(it);
  return true;
}

void DevToolsIOContext::DiscardAllStreams() {
  for (auto& entry : streams_)
    entry.second->Close(true);
  return streams_.clear();
}

}  // namespace content
