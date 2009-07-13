// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/metadata_url_request.h"

#include "base/message_loop.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace {

const char kMetadataScheme[] = "metadata";

class MetadataRequestHandler : public URLRequestJob {
 public:
  explicit MetadataRequestHandler(URLRequest* request);
  ~MetadataRequestHandler();

  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

  // URLRequestJob implementation.
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;

 private:
  void StartAsync();

  int data_offset_;

  DISALLOW_COPY_AND_ASSIGN(MetadataRequestHandler);
};

MetadataRequestHandler::MetadataRequestHandler(URLRequest* request)
    : URLRequestJob(request),
      data_offset_(0) {
}

MetadataRequestHandler::~MetadataRequestHandler() {
}

URLRequestJob* MetadataRequestHandler::Factory(URLRequest* request,
                                               const std::string& scheme) {
  return new MetadataRequestHandler(request);
}

void MetadataRequestHandler::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &MetadataRequestHandler::StartAsync));
}

void MetadataRequestHandler::Kill() {
}

bool MetadataRequestHandler::ReadRawData(net::IOBuffer* buf, int buf_size,
                                         int *bytes_read) {
  std::string fake_result = "{ \"foo\": 3.14 }";

  int remaining = static_cast<int>(fake_result.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), &fake_result[data_offset_], buf_size);
    data_offset_ += buf_size;
  }
  *bytes_read = buf_size;
  return true;
}

bool MetadataRequestHandler::GetMimeType(std::string* mime_type) const {
  *mime_type = "application/json";
  return true;
}

void MetadataRequestHandler::StartAsync() {
  NotifyHeadersComplete();
}

}  // namespace

void RegisterMetadataURLRequestHandler() {
  // This is currently unfinished. It will eventually provide metadata
  // but currently just returns a dummy stub string.
#if defined(OS_CHROMEOS)
  URLRequest::RegisterProtocolFactory(kMetadataScheme,
                                      &MetadataRequestHandler::Factory);
  url_util::AddStandardScheme(kMetadataScheme);
#endif
}
