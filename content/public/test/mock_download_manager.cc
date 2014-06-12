// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_manager.h"

#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"

namespace content {

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    uint32 id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& mime_type,
    const std::string& original_mime_type,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64 received_bytes,
    int64 total_bytes,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened)
    : id(id),
      current_path(current_path),
      target_path(target_path),
      url_chain(url_chain),
      referrer_url(referrer_url),
      mime_type(mime_type),
      original_mime_type(original_mime_type),
      start_time(start_time),
      end_time(end_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      state(state),
      danger_type(danger_type),
      interrupt_reason(interrupt_reason),
      opened(opened) {}

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    const CreateDownloadItemAdapter& rhs)
    : id(rhs.id),
      current_path(rhs.current_path),
      target_path(rhs.target_path),
      url_chain(rhs.url_chain),
      referrer_url(rhs.referrer_url),
      start_time(rhs.start_time),
      end_time(rhs.end_time),
      etag(rhs.etag),
      last_modified(rhs.last_modified),
      received_bytes(rhs.received_bytes),
      total_bytes(rhs.total_bytes),
      state(rhs.state),
      danger_type(rhs.danger_type),
      interrupt_reason(rhs.interrupt_reason),
      opened(rhs.opened) {}

MockDownloadManager::CreateDownloadItemAdapter::~CreateDownloadItemAdapter() {}

bool MockDownloadManager::CreateDownloadItemAdapter::operator==(
    const CreateDownloadItemAdapter& rhs) {
  return (id == rhs.id &&
          current_path == rhs.current_path &&
          target_path == rhs.target_path &&
          url_chain == rhs.url_chain &&
          referrer_url == rhs.referrer_url &&
          mime_type == rhs.mime_type &&
          original_mime_type == rhs.original_mime_type &&
          start_time == rhs.start_time &&
          end_time == rhs.end_time &&
          etag == rhs.etag &&
          last_modified == rhs.last_modified &&
          received_bytes == rhs.received_bytes &&
          total_bytes == rhs.total_bytes &&
          state == rhs.state &&
          danger_type == rhs.danger_type &&
          interrupt_reason == rhs.interrupt_reason &&
          opened == rhs.opened);
}

MockDownloadManager::MockDownloadManager() {}

MockDownloadManager::~MockDownloadManager() {}

void MockDownloadManager::StartDownload(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  MockStartDownload(info.get(), stream.get());
}

DownloadItem* MockDownloadManager::CreateDownloadItem(
    uint32 id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& mime_type,
    const std::string& original_mime_type,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64 received_bytes,
    int64 total_bytes,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened) {
  CreateDownloadItemAdapter adapter(
      id, current_path, target_path, url_chain, referrer_url, mime_type,
      original_mime_type, start_time, end_time, etag, last_modified,
      received_bytes, total_bytes, state, danger_type, interrupt_reason,
      opened);
  return MockCreateDownloadItem(adapter);
}

}  // namespace content
