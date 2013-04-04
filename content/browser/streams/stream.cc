// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "content/browser/streams/stream_handle_impl.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "net/base/io_buffer.h"

namespace {
// Start throttling the connection at about 1MB.
const size_t kDeferSizeThreshold = 40 * 32768;
}

namespace content {

Stream::Stream(StreamRegistry* registry,
               StreamWriteObserver* write_observer,
               const GURL& security_origin,
               const GURL& url)
    : data_bytes_read_(0),
      can_add_data_(true),
      security_origin_(security_origin),
      url_(url),
      data_length_(0),
      registry_(registry),
      read_observer_(NULL),
      write_observer_(write_observer),
      stream_handle_(NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  CreateByteStream(base::MessageLoopProxy::current(),
                   base::MessageLoopProxy::current(),
                   kDeferSizeThreshold,
                   &writer_,
                   &reader_);

  // Setup callback for writing.
  writer_->RegisterCallback(base::Bind(&Stream::OnSpaceAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));
  reader_->RegisterCallback(base::Bind(&Stream::OnDataAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));

  registry_->RegisterStream(this);
}

Stream::~Stream() {
}

bool Stream::SetReadObserver(StreamReadObserver* observer) {
  if (read_observer_)
    return false;
  read_observer_ = observer;
  return true;
}

void Stream::RemoveReadObserver(StreamReadObserver* observer) {
  DCHECK(observer == read_observer_);
  read_observer_ = NULL;
}

void Stream::RemoveWriteObserver(StreamWriteObserver* observer) {
  DCHECK(observer == write_observer_);
  write_observer_ = NULL;
}

void Stream::AddData(scoped_refptr<net::IOBuffer> buffer, size_t size) {
  can_add_data_ = writer_->Write(buffer, size);
}

void Stream::Finalize() {
  writer_->Close(DOWNLOAD_INTERRUPT_REASON_NONE);
  writer_.reset(NULL);

  // Continue asynchronously.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&Stream::OnDataAvailable, weak_ptr_factory_.GetWeakPtr()));
}

Stream::StreamState Stream::ReadRawData(net::IOBuffer* buf,
                                        int buf_size,
                                        int* bytes_read) {
  *bytes_read = 0;
  if (!data_) {
    data_length_ = 0;
    data_bytes_read_ = 0;
    ByteStreamReader::StreamState state = reader_->Read(&data_, &data_length_);
    switch (state) {
      case ByteStreamReader::STREAM_HAS_DATA:
        break;
      case ByteStreamReader::STREAM_COMPLETE:
        registry_->UnregisterStream(url());
        return STREAM_COMPLETE;
      case ByteStreamReader::STREAM_EMPTY:
        return STREAM_EMPTY;
    }
  }

  const size_t remaining_bytes = data_length_ - data_bytes_read_;
  size_t to_read =
      static_cast<size_t>(buf_size) < remaining_bytes ?
      buf_size : remaining_bytes;
  memcpy(buf->data(), data_->data() + data_bytes_read_, to_read);
  data_bytes_read_ += to_read;
  if (data_bytes_read_ >= data_length_)
    data_ = NULL;

  *bytes_read = to_read;
  return STREAM_HAS_DATA;
}

scoped_ptr<StreamHandle> Stream::CreateHandle(const GURL& original_url,
                                              const std::string& mime_type) {
  CHECK(!stream_handle_);
  stream_handle_ = new StreamHandleImpl(weak_ptr_factory_.GetWeakPtr(),
                                        original_url,
                                        mime_type);
  return scoped_ptr<StreamHandle>(stream_handle_).Pass();
}

void Stream::CloseHandle() {
  // Prevent deletion until this function ends.
  scoped_refptr<Stream> ref(this);

  CHECK(stream_handle_);
  stream_handle_ = NULL;
  registry_->UnregisterStream(url());
  if (write_observer_)
    write_observer_->OnClose(this);
}

void Stream::OnSpaceAvailable() {
  can_add_data_ = true;
  if (write_observer_)
    write_observer_->OnSpaceAvailable(this);
}

void Stream::OnDataAvailable() {
  if (read_observer_)
    read_observer_->OnDataAvailable(this);
}

}  // namespace content

