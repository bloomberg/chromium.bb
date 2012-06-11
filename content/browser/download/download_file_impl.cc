// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_impl.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/power_save_blocker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/browser/download/download_stats.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;
using content::DownloadId;
using content::DownloadManager;

const int kUpdatePeriodMs = 500;
const int kMaxTimeBlockingFileThreadMs = 1000;

DownloadFileImpl::DownloadFileImpl(
    const DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    DownloadRequestHandleInterface* request_handle,
    DownloadManager* download_manager,
    bool calculate_hash,
    scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
    const net::BoundNetLog& bound_net_log)
        : file_(info->save_info.file_path,
                info->url(),
                info->referrer_url,
                info->received_bytes,
                calculate_hash,
                info->save_info.hash_state,
                info->save_info.file_stream,
                bound_net_log),
          stream_reader_(stream.Pass()),
          id_(info->download_id),
          request_handle_(request_handle),
          download_manager_(download_manager),
          bytes_seen_(0),
          bound_net_log_(bound_net_log),
          weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
          power_save_blocker_(power_save_blocker.Pass()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

DownloadFileImpl::~DownloadFileImpl() {
}

// BaseFile delegated functions.
net::Error DownloadFileImpl::Initialize() {
  update_timer_.reset(new base::RepeatingTimer<DownloadFileImpl>());
  net::Error result = file_.Initialize();
  if (result != net::OK)
    return result;

  stream_reader_->RegisterCallback(
      base::Bind(&DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr()));

  download_start_ = base::TimeTicks::Now();
  // Initial pull from the straw.
  StreamActive();

  return result;
}

net::Error DownloadFileImpl::AppendDataToFile(const char* data,
                                              size_t data_len) {
  if (!update_timer_->IsRunning()) {
    update_timer_->Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                         this, &DownloadFileImpl::SendUpdate);
  }
  return file_.AppendDataToFile(data, data_len);
}

net::Error DownloadFileImpl::Rename(const FilePath& full_path) {
  return file_.Rename(full_path);
}

void DownloadFileImpl::Detach() {
  file_.Detach();
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

void DownloadFileImpl::AnnotateWithSourceInformation() {
  file_.AnnotateWithSourceInformation();
}

FilePath DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

int64 DownloadFileImpl::BytesSoFar() const {
  return file_.bytes_so_far();
}

int64 DownloadFileImpl::CurrentSpeed() const {
  return file_.CurrentSpeed();
}

bool DownloadFileImpl::GetHash(std::string* hash) {
  return file_.GetHash(hash);
}

std::string DownloadFileImpl::GetHashState() {
  return file_.GetHashState();
}

// DownloadFileInterface implementation.
void DownloadFileImpl::CancelDownloadRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  request_handle_->CancelRequest();
}

int DownloadFileImpl::Id() const {
  return id_.local();
}

DownloadManager* DownloadFileImpl::GetDownloadManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return download_manager_.get();
}

const DownloadId& DownloadFileImpl::GlobalId() const {
  return id_;
}

std::string DownloadFileImpl::DebugString() const {
  return base::StringPrintf("{"
                            " id_ = " "%d"
                            " request_handle = %s"
                            " Base File = %s"
                            " }",
                            id_.local(),
                            request_handle_->DebugString().c_str(),
                            file_.DebugString().c_str());
}

void DownloadFileImpl::StreamActive() {
  base::TimeTicks start(base::TimeTicks::Now());
  base::TimeTicks now;
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  size_t total_incoming_data_size = 0;
  size_t num_buffers = 0;
  content::ByteStreamReader::StreamState state(
      content::ByteStreamReader::STREAM_EMPTY);
  content::DownloadInterruptReason reason =
      content::DOWNLOAD_INTERRUPT_REASON_NONE;
  base::TimeDelta delta(
      base::TimeDelta::FromMilliseconds(kMaxTimeBlockingFileThreadMs));

  // Take care of any file local activity required.
  do {
    state = stream_reader_->Read(&incoming_data, &incoming_data_size);

    net::Error result = net::OK;
    switch (state) {
      case content::ByteStreamReader::STREAM_EMPTY:
        break;
      case content::ByteStreamReader::STREAM_HAS_DATA:
        {
          ++num_buffers;
          base::TimeTicks write_start(base::TimeTicks::Now());
          result = AppendDataToFile(
              incoming_data.get()->data(), incoming_data_size);
          disk_writes_time_ += (base::TimeTicks::Now() - write_start);
          total_incoming_data_size += incoming_data_size;
          reason = content::ConvertNetErrorToInterruptReason(
              result, content::DOWNLOAD_INTERRUPT_FROM_DISK);
        }
        break;
      case content::ByteStreamReader::STREAM_COMPLETE:
        {
          reason = stream_reader_->GetStatus();
          SendUpdate();
          base::TimeTicks close_start(base::TimeTicks::Now());
          file_.Finish();
          base::TimeTicks now(base::TimeTicks::Now());
          disk_writes_time_ += (now - close_start);
          download_stats::RecordFileBandwidth(
              bytes_seen_, disk_writes_time_, now - download_start_);
          update_timer_.reset();
        }
        break;
      default:
        NOTREACHED();
        break;
    }
    now = base::TimeTicks::Now();
  } while (state == content::ByteStreamReader::STREAM_HAS_DATA &&
           reason == content::DOWNLOAD_INTERRUPT_REASON_NONE &&
           now - start <= delta);

  // If we're stopping to yield the thread, post a task so we come back.
  if (state == content::ByteStreamReader::STREAM_HAS_DATA &&
      now - start > delta) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileImpl::StreamActive,
                   weak_factory_.GetWeakPtr()));
  }

  if (total_incoming_data_size)
    download_stats::RecordFileThreadReceiveBuffers(num_buffers);
  bytes_seen_ += total_incoming_data_size;

  download_stats::RecordContiguousWriteTime(now - start);

  // Take care of communication with our controller.
  if (reason != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Error case for both upstream source and file write.
    // Shut down processing and signal an error to our controller.
    // Our controller will clean us up.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    if (download_manager_.get()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::OnDownloadInterrupted,
                     download_manager_, id_.local(),
                     BytesSoFar(), GetHashState(), reason));
    }
  } else if (state == content::ByteStreamReader::STREAM_COMPLETE) {
    // Signal successful completion and shut down processing.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    if (download_manager_.get()) {
      std::string hash;
      if (!GetHash(&hash) || file_.IsEmptyHash(hash))
        hash.clear();
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::OnResponseCompleted,
                     download_manager_, id_.local(),
                     BytesSoFar(), hash));
    }
  }
  if (bound_net_log_.IsLoggingAllEvents()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_STREAM_DRAINED,
        make_scoped_refptr(new download_net_logs::FileStreamDrainedParameters(
            total_incoming_data_size, num_buffers)));
  }
}

void DownloadFileImpl::SendUpdate() {
  if (download_manager_.get()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadManager::UpdateDownload,
                   download_manager_, id_.local(),
                   BytesSoFar(), CurrentSpeed(), GetHashState()));
  }
}
