// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_manager.h"

#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"

namespace content {

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const base::Time& start_time,
    const base::Time& end_time,
    int64 received_bytes,
    int64 total_bytes,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened)
    : current_path(current_path),
      target_path(target_path),
      url_chain(url_chain),
      referrer_url(referrer_url),
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
    : current_path(rhs.current_path),
      target_path(rhs.target_path),
      url_chain(rhs.url_chain),
      referrer_url(rhs.referrer_url),
      start_time(rhs.start_time),
      end_time(rhs.end_time),
      received_bytes(rhs.received_bytes),
      total_bytes(rhs.total_bytes),
      state(rhs.state),
      danger_type(rhs.danger_type),
      interrupt_reason(rhs.interrupt_reason),
      opened(rhs.opened) {}

MockDownloadManager::CreateDownloadItemAdapter::~CreateDownloadItemAdapter() {}

bool MockDownloadManager::CreateDownloadItemAdapter::operator==(
    const CreateDownloadItemAdapter& rhs) {
  return (current_path == rhs.current_path &&
          target_path == rhs.target_path &&
          url_chain == rhs.url_chain &&
          referrer_url == rhs.referrer_url &&
          start_time == rhs.start_time &&
          end_time == rhs.end_time &&
          received_bytes == rhs.received_bytes &&
          total_bytes == rhs.total_bytes &&
          state == rhs.state &&
          danger_type == rhs.danger_type &&
          interrupt_reason == rhs.interrupt_reason &&
          opened == rhs.opened);
}

MockDownloadManager::MockDownloadManager() {}

MockDownloadManager::~MockDownloadManager() {}

DownloadItem* MockDownloadManager::StartDownload(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream) {
  return MockStartDownload(info.get(), stream.get());
}

DownloadItem* MockDownloadManager::CreateDownloadItem(
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const base::Time& start_time,
    const base::Time& end_time,
    int64 received_bytes,
    int64 total_bytes,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened) {
  CreateDownloadItemAdapter adapter(
      current_path, target_path, url_chain, referrer_url, start_time,
      end_time, received_bytes, total_bytes, state, danger_type,
      interrupt_reason, opened);
  return MockCreateDownloadItem(adapter);
}

}  // namespace content
