// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/ipc_data_source.h"

#include "chrome/common/chrome_utility_messages.h"
#include "content/public/utility/utility_thread.h"

namespace metadata {

IPCDataSource::IPCDataSource(int64 total_size)
    : total_size_(total_size),
      next_request_id_(0) {
}

IPCDataSource::~IPCDataSource() {}

void IPCDataSource::set_host(media::DataSourceHost* host) {
  DataSource::set_host(host);
  if (media::DataSource::host()) {
    media::DataSource::host()->SetTotalBytes(total_size_);
  }
}

void IPCDataSource::Stop(const base::Closure& callback) {
  callback.Run();
}

void IPCDataSource::Read(int64 position, int size, uint8* data,
                         const DataSource::ReadCB& read_cb) {
  CHECK_GE(total_size_, 0);
  CHECK_GE(position, 0);
  CHECK_GE(size, 0);

  // Cap position and size within bounds.
  position = std::min(position, total_size_);
  int64 clamped_size =
      std::min(static_cast<int64>(size), total_size_ - position);

  int64 request_id = ++next_request_id_;

  Request request;
  request.destination = data;
  request.callback = read_cb;

  pending_requests_[request_id] = request;
  content::UtilityThread::Get()->Send(new ChromeUtilityHostMsg_RequestBlobBytes(
      request_id, position, clamped_size));
}

bool IPCDataSource::GetSize(int64* size_out) {
  *size_out = total_size_;
  return true;
}

bool IPCDataSource::IsStreaming() {
  return false;
}

void IPCDataSource::SetBitrate(int bitrate) {
}

bool IPCDataSource::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IPCDataSource, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RequestBlobBytes_Finished,
                        OnRequestBlobBytesFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

IPCDataSource::Request::Request()
    : destination(NULL) {
}

IPCDataSource::Request::~Request() {
}

void IPCDataSource::OnRequestBlobBytesFinished(int64 request_id,
                                               const std::string& bytes) {
  std::map<int64, Request>::iterator it = pending_requests_.find(request_id);

  if (it == pending_requests_.end())
    return;

  std::copy(bytes.begin(), bytes.end(), it->second.destination);
  it->second.callback.Run(bytes.size());

  pending_requests_.erase(it);
}

}  // namespace metadata
