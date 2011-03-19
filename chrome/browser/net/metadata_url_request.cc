// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/metadata_url_request.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/parsers/metadata_parser_manager.h"
#include "chrome/browser/parsers/metadata_parser.h"
#include "chrome/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace {

class MetadataRequestHandler : public net::URLRequestJob {
 public:
  explicit MetadataRequestHandler(net::URLRequest* request);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // net::URLRequestJob implementation.
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;

 private:
  ~MetadataRequestHandler();

  void StartAsync();
  std::string result_;
  bool parsed;
  int data_offset_;
  ScopedRunnableMethodFactory<MetadataRequestHandler> method_factory_;
  DISALLOW_COPY_AND_ASSIGN(MetadataRequestHandler);
};

MetadataRequestHandler::MetadataRequestHandler(net::URLRequest* request)
    : net::URLRequestJob(request),
      data_offset_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  parsed = false;
}

MetadataRequestHandler::~MetadataRequestHandler() {
}

net::URLRequestJob* MetadataRequestHandler::Factory(net::URLRequest* request,
                                               const std::string& scheme) {
  return new MetadataRequestHandler(request);
}

void MetadataRequestHandler::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&MetadataRequestHandler::StartAsync));
}

void MetadataRequestHandler::Kill() {
}

bool MetadataRequestHandler::ReadRawData(net::IOBuffer* buf, int buf_size,
                                         int *bytes_read) {
  FilePath path;

  if (!request()->url().is_valid()) {
    return false;
  }
  if (!net::FileURLToFilePath(request()->url(), &path)) {
    return false;
  }
  if (!parsed) {
    MetadataParserManager* manager = MetadataParserManager::GetInstance();
    scoped_ptr<MetadataParser> parser(manager->GetParserForFile(path));
    if (parser != NULL) {
      result_ = "{\n";
      parser->Parse();
      MetadataPropertyIterator *iter = parser->GetPropertyIterator();
      while (!iter->IsEnd()) {
        std::string key;
        std::string value;
        if (iter->GetNext(&key, &value)) {
          result_ += "\"";
          result_ += key;
          result_ += "\":";
          result_ += "\"";
          result_ += value;
          result_ += "\",\n";
        } else {
          break;
        }
      }
      result_ += "}";
      delete iter;
    } else {
      result_ = "{}";
    }
    parsed = true;
  }
  int remaining = static_cast<int>(result_.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), &result_[data_offset_], buf_size);
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
#if defined(OS_CHROMEOS)
  net::URLRequest::RegisterProtocolFactory(chrome::kMetadataScheme,
                                           &MetadataRequestHandler::Factory);
#endif
}
