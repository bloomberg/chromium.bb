// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/io_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace protocol {

IOHandler::IOHandler(DevToolsIOContext* io_context)
    : DevToolsDomainHandler(IO::Metainfo::domainName),
      io_context_(io_context)
    , weak_factory_(this) {}

IOHandler::~IOHandler() {}

void IOHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new IO::Frontend(dispatcher->channel()));
  IO::Dispatcher::wire(dispatcher, this);
}

void IOHandler::Read(
    const std::string& handle,
    Maybe<int> offset,
    Maybe<int> max_size,
    std::unique_ptr<ReadCallback> callback) {
  static const size_t kDefaultChunkSize = 10 * 1024 * 1024;

  scoped_refptr<DevToolsIOContext::Stream> stream =
      io_context_->GetByHandle(handle);
  if (!stream) {
    callback->sendFailure(Response::InvalidParams("Invalid stream handle"));
    return;
  }
  stream->Read(
      offset.fromMaybe(-1), max_size.fromMaybe(kDefaultChunkSize),
      base::BindOnce(&IOHandler::ReadComplete, weak_factory_.GetWeakPtr(),
                     base::Passed(std::move(callback))));
}

void IOHandler::ReadComplete(std::unique_ptr<ReadCallback> callback,
                             std::unique_ptr<std::string> data,
                             int status) {
  if (status == DevToolsIOContext::Stream::StatusFailure) {
    callback->sendFailure(Response::Error("Read failed"));
    return;
  }
  bool eof = status == DevToolsIOContext::Stream::StatusEOF;
  callback->sendSuccess(std::move(*data), eof);
}

Response IOHandler::Close(const std::string& handle) {
  return io_context_->Close(handle) ? Response::OK()
      : Response::InvalidParams("Invalid stream handle");
}

}  // namespace protocol
}  // namespace content
