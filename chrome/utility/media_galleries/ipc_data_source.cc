// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/ipc_data_source.h"

#include "base/message_loop/message_loop_proxy.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/utility/utility_thread.h"

namespace metadata {

IPCDataSource::IPCDataSource(int64 total_size)
    : total_size_(total_size),
      utility_task_runner_(base::MessageLoopProxy::current()),
      next_request_id_(0) {
  data_source_thread_checker_.DetachFromThread();
}

IPCDataSource::~IPCDataSource() {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
}

void IPCDataSource::Stop(const base::Closure& callback) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  callback.Run();
}

void IPCDataSource::Read(int64 position, int size, uint8* data,
                         const DataSource::ReadCB& read_cb) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  utility_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IPCDataSource::ReadOnUtilityThread, base::Unretained(this),
                 position, size, data, read_cb));
}

bool IPCDataSource::GetSize(int64* size_out) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  *size_out = total_size_;
  return true;
}

bool IPCDataSource::IsStreaming() {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  return false;
}

void IPCDataSource::SetBitrate(int bitrate) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
}

bool IPCDataSource::OnMessageReceived(const IPC::Message& message) {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
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

void IPCDataSource::ReadOnUtilityThread(int64 position, int size, uint8* data,
                                        const DataSource::ReadCB& read_cb) {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
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

void IPCDataSource::OnRequestBlobBytesFinished(int64 request_id,
                                               const std::string& bytes) {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
  std::map<int64, Request>::iterator it = pending_requests_.find(request_id);

  if (it == pending_requests_.end())
    return;

  std::copy(bytes.begin(), bytes.end(), it->second.destination);
  it->second.callback.Run(bytes.size());

  pending_requests_.erase(it);
}

}  // namespace metadata
