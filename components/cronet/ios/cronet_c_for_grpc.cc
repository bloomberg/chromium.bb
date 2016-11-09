// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/cronet_c_for_grpc.h"

#include <stdbool.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/cronet/ios/cronet_bidirectional_stream.h"
#include "components/cronet/ios/cronet_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/bidirectional_stream.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_header_block.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace {

class HeadersArray : public cronet_bidirectional_stream_header_array {
 public:
  HeadersArray(const net::SpdyHeaderBlock& header_block);
  ~HeadersArray();

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadersArray);
  base::StringPairs headers_strings_;
};

HeadersArray::HeadersArray(const net::SpdyHeaderBlock& header_block)
    : headers_strings_(header_block.size()) {
  // Count and headers are inherited from parent structure.
  count = capacity = header_block.size();
  headers = new cronet_bidirectional_stream_header[count];
  size_t i = 0;
  // Copy headers into |headers_strings_| because string pieces are not
  // '\0'-terminated.
  for (const auto& it : header_block) {
    headers_strings_[i].first = it.first.as_string();
    headers_strings_[i].second = it.second.as_string();
    headers[i].key = headers_strings_[i].first.c_str();
    headers[i].value = headers_strings_[i].second.c_str();
    ++i;
  }
}

HeadersArray::~HeadersArray() {
  delete[] headers;
}

class CronetBidirectionalStreamAdapter
    : public cronet::CronetBidirectionalStream::Delegate {
 public:
  CronetBidirectionalStreamAdapter(
      cronet_engine* engine,
      void* annotation,
      cronet_bidirectional_stream_callback* callback);

  virtual ~CronetBidirectionalStreamAdapter();

  void OnStreamReady() override;

  void OnHeadersReceived(const net::SpdyHeaderBlock& headers_block,
                         const char* negotiated_protocol) override;

  void OnDataRead(char* data, int size) override;

  void OnDataSent(const char* data) override;

  void OnTrailersReceived(const net::SpdyHeaderBlock& trailers_block) override;

  void OnSucceeded() override;

  void OnFailed(int error) override;

  void OnCanceled() override;

  cronet_bidirectional_stream* c_stream() const { return c_stream_.get(); }

  static cronet::CronetBidirectionalStream* GetCronetStream(
      cronet_bidirectional_stream* stream);

  static void DestroyAdapterForStream(cronet_bidirectional_stream* stream);

 private:
  void DestroyOnNetworkThread();

  // None of these objects are owned by |this|.
  cronet::CronetEnvironment* cronet_environment_;
  cronet::CronetBidirectionalStream* cronet_bidirectional_stream_;
  // C side
  std::unique_ptr<cronet_bidirectional_stream> c_stream_;
  cronet_bidirectional_stream_callback* c_callback_;
};

CronetBidirectionalStreamAdapter::CronetBidirectionalStreamAdapter(
    cronet_engine* engine,
    void* annotation,
    cronet_bidirectional_stream_callback* callback)
    : cronet_environment_(
          reinterpret_cast<cronet::CronetEnvironment*>(engine->obj)),
      c_stream_(base::MakeUnique<cronet_bidirectional_stream>()),
      c_callback_(callback) {
  DCHECK(cronet_environment_);
  cronet_bidirectional_stream_ =
      new cronet::CronetBidirectionalStream(cronet_environment_, this);
  c_stream_->obj = this;
  c_stream_->annotation = annotation;
}

CronetBidirectionalStreamAdapter::~CronetBidirectionalStreamAdapter() {}

void CronetBidirectionalStreamAdapter::OnStreamReady() {
  DCHECK(c_callback_->on_stream_ready);
  c_callback_->on_stream_ready(c_stream());
}

void CronetBidirectionalStreamAdapter::OnHeadersReceived(
    const net::SpdyHeaderBlock& headers_block,
    const char* negotiated_protocol) {
  DCHECK(c_callback_->on_response_headers_received);
  HeadersArray response_headers(headers_block);
  c_callback_->on_response_headers_received(c_stream(), &response_headers,
                                            negotiated_protocol);
}

void CronetBidirectionalStreamAdapter::OnDataRead(char* data, int size) {
  DCHECK(c_callback_->on_read_completed);
  c_callback_->on_read_completed(c_stream(), data, size);
}

void CronetBidirectionalStreamAdapter::OnDataSent(const char* data) {
  DCHECK(c_callback_->on_write_completed);
  c_callback_->on_write_completed(c_stream(), data);
}

void CronetBidirectionalStreamAdapter::OnTrailersReceived(
    const net::SpdyHeaderBlock& trailers_block) {
  DCHECK(c_callback_->on_response_trailers_received);
  HeadersArray response_trailers(trailers_block);
  c_callback_->on_response_trailers_received(c_stream(), &response_trailers);
}

void CronetBidirectionalStreamAdapter::OnSucceeded() {
  DCHECK(c_callback_->on_succeded);
  c_callback_->on_succeded(c_stream());
}

void CronetBidirectionalStreamAdapter::OnFailed(int error) {
  DCHECK(c_callback_->on_failed);
  c_callback_->on_failed(c_stream(), error);
}

void CronetBidirectionalStreamAdapter::OnCanceled() {
  DCHECK(c_callback_->on_canceled);
  c_callback_->on_canceled(c_stream());
}

cronet::CronetBidirectionalStream*
CronetBidirectionalStreamAdapter::GetCronetStream(
    cronet_bidirectional_stream* stream) {
  DCHECK(stream);
  CronetBidirectionalStreamAdapter* adapter =
      static_cast<CronetBidirectionalStreamAdapter*>(stream->obj);
  DCHECK(adapter->c_stream() == stream);
  DCHECK(adapter->cronet_bidirectional_stream_);
  return adapter->cronet_bidirectional_stream_;
}

void CronetBidirectionalStreamAdapter::DestroyAdapterForStream(
    cronet_bidirectional_stream* stream) {
  DCHECK(stream);
  CronetBidirectionalStreamAdapter* adapter =
      static_cast<CronetBidirectionalStreamAdapter*>(stream->obj);
  DCHECK(adapter->c_stream() == stream);
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete.
  adapter->cronet_bidirectional_stream_->Destroy();
  adapter->cronet_environment_->PostToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStreamAdapter::DestroyOnNetworkThread,
                 base::Unretained(adapter)));
}

void CronetBidirectionalStreamAdapter::DestroyOnNetworkThread() {
  DCHECK(cronet_environment_->IsOnNetworkThread());
  delete this;
}

}  // namespace

cronet_bidirectional_stream* cronet_bidirectional_stream_create(
    cronet_engine* engine,
    void* annotation,
    cronet_bidirectional_stream_callback* callback) {
  // Allocate new C++ adapter that will invoke |callback|.
  CronetBidirectionalStreamAdapter* stream_adapter =
      new CronetBidirectionalStreamAdapter(engine, annotation, callback);
  return stream_adapter->c_stream();
}

int cronet_bidirectional_stream_destroy(cronet_bidirectional_stream* stream) {
  CronetBidirectionalStreamAdapter::DestroyAdapterForStream(stream);
  return 1;
}

void cronet_bidirectional_stream_disable_auto_flush(
    cronet_bidirectional_stream* stream,
    bool disable_auto_flush) {
  CronetBidirectionalStreamAdapter::GetCronetStream(stream)->disable_auto_flush(
      disable_auto_flush);
}

void cronet_bidirectional_stream_delay_request_headers_until_flush(
    cronet_bidirectional_stream* stream,
    bool delay_headers_until_flush) {
  CronetBidirectionalStreamAdapter::GetCronetStream(stream)
      ->delay_headers_until_flush(delay_headers_until_flush);
}

int cronet_bidirectional_stream_start(
    cronet_bidirectional_stream* stream,
    const char* url,
    int priority,
    const char* method,
    const cronet_bidirectional_stream_header_array* headers,
    bool end_of_stream) {
  cronet::CronetBidirectionalStream* cronet_stream =
      CronetBidirectionalStreamAdapter::GetCronetStream(stream);
  net::HttpRequestHeaders request_headers;
  if (headers) {
    for (size_t i = 0; i < headers->count; ++i) {
      std::string name(headers->headers[i].key);
      std::string value(headers->headers[i].value);
      if (!net::HttpUtil::IsValidHeaderName(name) ||
          !net::HttpUtil::IsValidHeaderValue(value)) {
        DLOG(ERROR) << "Invalid Header " << name << "=" << value;
        return i + 1;
      }
      request_headers.SetHeader(name, value);
    }
  }
  return cronet_stream->Start(url, priority, method, request_headers,
                              end_of_stream);
}

int cronet_bidirectional_stream_read(cronet_bidirectional_stream* stream,
                                     char* buffer,
                                     int capacity) {
  return CronetBidirectionalStreamAdapter::GetCronetStream(stream)->ReadData(
      buffer, capacity);
}

int cronet_bidirectional_stream_write(cronet_bidirectional_stream* stream,
                                      const char* buffer,
                                      int count,
                                      bool end_of_stream) {
  return CronetBidirectionalStreamAdapter::GetCronetStream(stream)->WriteData(
      buffer, count, end_of_stream);
}

void cronet_bidirectional_stream_flush(cronet_bidirectional_stream* stream) {
  return CronetBidirectionalStreamAdapter::GetCronetStream(stream)->Flush();
}

void cronet_bidirectional_stream_cancel(cronet_bidirectional_stream* stream) {
  CronetBidirectionalStreamAdapter::GetCronetStream(stream)->Cancel();
}
