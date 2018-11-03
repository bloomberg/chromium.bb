// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_response_body_consumer.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "net/base/io_buffer.h"
#include "net/filter/zlib_stream_wrapper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

constexpr uint32_t URLResponseBodyConsumer::kMaxNumConsumedBytesInTask;
const size_t kInflateBufferSize = 4 * 1024;

class URLResponseBodyConsumer::ReclaimAccountant
    : public base::RefCounted<URLResponseBodyConsumer::ReclaimAccountant> {
 public:
  explicit ReclaimAccountant(scoped_refptr<URLResponseBodyConsumer> consumer)
      : consumer_(consumer) {}

  void Reclaim(uint32_t reclaim_bytes) { reclaim_length_ += reclaim_bytes; }

 private:
  friend class base::RefCounted<URLResponseBodyConsumer::ReclaimAccountant>;

  ~ReclaimAccountant() { consumer_->Reclaim(reclaim_length_); }

  scoped_refptr<URLResponseBodyConsumer> consumer_;
  uint32_t reclaim_length_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReclaimAccountant);
};

class URLResponseBodyConsumer::ReceivedData final
    : public RequestPeer::ReceivedData {
 public:
  ReceivedData(const char* payload,
               int payload_length,
               int reclaim_length,
               scoped_refptr<ReclaimAccountant> reclaim_accountant)
      : payload_(payload),
        payload_length_(payload_length),
        reclaim_length_(reclaim_length),
        reclaim_accountant_(reclaim_accountant) {}

  ~ReceivedData() override { reclaim_accountant_->Reclaim(reclaim_length_); }

  const char* payload() const override { return payload_; }
  int length() const override { return payload_length_; }

 private:
  const char* const payload_;
  const uint32_t payload_length_;
  const uint32_t reclaim_length_;
  scoped_refptr<ReclaimAccountant> reclaim_accountant_;

  DISALLOW_COPY_AND_ASSIGN(ReceivedData);
};

URLResponseBodyConsumer::URLResponseBodyConsumer(
    int request_id,
    ResourceDispatcher* resource_dispatcher,
    mojo::ScopedDataPipeConsumerHandle handle,
    bool inflate_response,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : request_id_(request_id),
      resource_dispatcher_(resource_dispatcher),
      handle_(std::move(handle)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner),
      task_runner_(task_runner),
      zlib_wrapper_(inflate_response
                        ? new net::ZLibStreamWrapper(
                              net::ZLibStreamWrapper::SourceType::kGzip)
                        : nullptr),
      inflate_buffer_(inflate_response ? new net::IOBuffer(kInflateBufferSize)
                                       : nullptr),
      has_seen_end_of_data_(!handle_.is_valid()) {
  if (zlib_wrapper_ && !zlib_wrapper_->Init()) {
    // If zlib can't be initialized then release the wrapper which will result
    // in the compressed response being received and unable to be processed.
    zlib_wrapper_.release();
    inflate_buffer_.reset();
  }
  handle_watcher_.Watch(
      handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&URLResponseBodyConsumer::OnReadable, base::Unretained(this)));
}

URLResponseBodyConsumer::~URLResponseBodyConsumer() {}

void URLResponseBodyConsumer::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (has_been_cancelled_)
    return;
  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

void URLResponseBodyConsumer::Cancel() {
  has_been_cancelled_ = true;
  handle_watcher_.Cancel();
}

void URLResponseBodyConsumer::SetDefersLoading() {
  is_deferred_ = true;
}

void URLResponseBodyConsumer::UnsetDefersLoading() {
  is_deferred_ = false;
  OnReadable(MOJO_RESULT_OK);
}

void URLResponseBodyConsumer::ArmOrNotify() {
  if (has_been_cancelled_)
    return;
  handle_watcher_.ArmOrNotify();
}

void URLResponseBodyConsumer::Reclaim(uint32_t size) {
  MojoResult result = handle_->EndReadData(size);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  if (is_in_on_readable_)
    return;

  handle_watcher_.ArmOrNotify();
}

void URLResponseBodyConsumer::OnReadable(MojoResult unused) {
  if (has_been_cancelled_ || has_seen_end_of_data_ || is_deferred_)
    return;

  DCHECK(!is_in_on_readable_);
  uint32_t num_bytes_consumed = 0;

  // Protect |this| as RequestPeer::OnReceivedData may call deref.
  scoped_refptr<URLResponseBodyConsumer> protect(this);
  base::AutoReset<bool> is_in_on_readable(&is_in_on_readable_, true);

  while (!has_been_cancelled_ && !is_deferred_) {
    const void* buffer = nullptr;
    uint32_t available = 0;
    MojoResult result =
        handle_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_BUSY)
      return;
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      has_seen_end_of_data_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      status_.error_code = net::ERR_FAILED;
      has_seen_end_of_data_ = true;
      has_received_completion_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    DCHECK_LE(num_bytes_consumed, kMaxNumConsumedBytesInTask);
    available =
        std::min(available, kMaxNumConsumedBytesInTask - num_bytes_consumed);
    if (available == 0) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      result = handle_->EndReadData(0);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      handle_watcher_.ArmOrNotify();
      return;
    }
    num_bytes_consumed += available;
    ResourceDispatcher::PendingRequestInfo* request_info =
        resource_dispatcher_->GetPendingRequestInfo(request_id_);
    DCHECK(request_info);

    scoped_refptr<ReclaimAccountant> reclaim_accountant =
        new ReclaimAccountant(this);
    if (zlib_wrapper_) {
      const char* input_buffer_ptr = static_cast<const char*>(buffer);
      int input_buffer_available = available;
      while (input_buffer_available) {
        scoped_refptr<net::WrappedIOBuffer> input_buffer =
            new net::WrappedIOBuffer(input_buffer_ptr);
        int consumed_bytes = 0;
        int output_bytes_written = zlib_wrapper_->FilterData(
            inflate_buffer_.get(), kInflateBufferSize, input_buffer.get(),
            input_buffer_available, &consumed_bytes, has_seen_end_of_data_);
        if (output_bytes_written < 0) {
          status_.error_code = output_bytes_written;
          has_seen_end_of_data_ = true;
          has_received_completion_ = true;
          NotifyCompletionIfAppropriate();
          return;
        }

        input_buffer_ptr += consumed_bytes;
        input_buffer_available -= consumed_bytes;
        DCHECK_GE(input_buffer_available, 0);

        if (output_bytes_written > 0) {
          request_info->peer->OnReceivedData(std::make_unique<ReceivedData>(
              inflate_buffer_->data(), output_bytes_written, consumed_bytes,
              reclaim_accountant));
        }
      }
    } else {
      request_info->peer->OnReceivedData(std::make_unique<ReceivedData>(
          static_cast<const char*>(buffer), available, available,
          reclaim_accountant));
    }
    reclaim_accountant.reset();
  }
}

void URLResponseBodyConsumer::NotifyCompletionIfAppropriate() {
  if (has_been_cancelled_)
    return;
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;
  // Cancel this instance in order not to notify twice.
  Cancel();

  resource_dispatcher_->OnRequestComplete(request_id_, status_);
  // |this| may be deleted.
}

}  // namespace content
