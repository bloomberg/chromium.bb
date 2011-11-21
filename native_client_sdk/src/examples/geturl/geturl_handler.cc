// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/geturl/geturl_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace {
bool IsError(int32_t result) {
  return ((PP_OK != result) && (PP_OK_COMPLETIONPENDING != result));
}
}  // namespace

GetURLHandler* GetURLHandler::Create(pp::Instance* instance,
                                     const std::string& url) {
  return new GetURLHandler(instance, url);
}

GetURLHandler::GetURLHandler(pp::Instance* instance,
                             const std::string& url)
    : instance_(instance),
      url_(url),
      url_request_(instance),
      url_loader_(instance),
      cc_factory_(this) {
  url_request_.SetURL(url);
  url_request_.SetMethod("GET");
}

GetURLHandler::~GetURLHandler() {
}

void GetURLHandler::Start() {
  pp::CompletionCallback cc =
      cc_factory_.NewRequiredCallback(&GetURLHandler::OnOpen);
  url_loader_.Open(url_request_, cc);
}

void GetURLHandler::OnOpen(int32_t result) {
  if (result != PP_OK) {
    ReportResultAndDie(url_, "pp::URLLoader::Open() failed", false);
    return;
  }
  // Here you would process the headers. A real program would want to at least
  // check the HTTP code and potentially cancel the request.
  // pp::URLResponseInfo response = loader_.GetResponseInfo();

  // Start streaming.
  ReadBody();
}

void GetURLHandler::AppendDataBytes(const char* buffer, int32_t num_bytes) {
  if (num_bytes <= 0)
    return;
  // Make sure we don't get a buffer overrun.
  num_bytes = std::min(READ_BUFFER_SIZE, num_bytes);
  url_response_body_.reserve(url_response_body_.size() + num_bytes);
  url_response_body_.insert(url_response_body_.end(),
                            buffer,
                            buffer + num_bytes);
}

void GetURLHandler::OnRead(int32_t result) {
  if (result == PP_OK) {
    // Streaming the file is complete.
    ReportResultAndDie(url_, url_response_body_, true);
  } else if (result > 0) {
    // The URLLoader just filled "result" number of bytes into our buffer.
    // Save them and perform another read.
    AppendDataBytes(buffer_, result);
    ReadBody();
  } else {
    // A read error occurred.
    ReportResultAndDie(url_,
                       "pp::URLLoader::ReadResponseBody() result<0",
                       false);
  }
}

void GetURLHandler::ReadBody() {
  // Note that you specifically want an "optional" callback here. This will
  // allow ReadBody() to return synchronously, ignoring your completion
  // callback, if data is available. For fast connections and large files,
  // reading as fast as we can will make a large performance difference
  // However, in the case of a synchronous return, we need to be sure to run
  // the callback we created since the loader won't do anything with it.
  pp::CompletionCallback cc =
      cc_factory_.NewOptionalCallback(&GetURLHandler::OnRead);
  int32_t result = PP_OK;
  do {
    result = url_loader_.ReadResponseBody(buffer_, sizeof(buffer_), cc);
    // Handle streaming data directly. Note that we *don't* want to call
    // OnRead here, since in the case of result > 0 it will schedule
    // another call to this function. If the network is very fast, we could
    // end up with a deeply recursive stack.
    if (result > 0) {
      AppendDataBytes(buffer_, result);
    }
  } while (result > 0);

  if (result != PP_OK_COMPLETIONPENDING) {
    // Either we reached the end of the stream (result == PP_OK) or there was
    // an error. We want OnRead to get called no matter what to handle
    // that case, whether the error is synchronous or asynchronous. If the
    // result code *is* COMPLETIONPENDING, our callback will be called
    // asynchronously.
    cc.Run(result);
  }
}

void GetURLHandler::ReportResultAndDie(const std::string& fname,
                                       const std::string& text,
                                       bool success) {
  ReportResult(fname, text, success);
  delete this;
}

void GetURLHandler::ReportResult(const std::string& fname,
                                 const std::string& text,
                                 bool success) {
  if (success)
    printf("GetURLHandler::ReportResult(Ok).\n");
  else
    printf("GetURLHandler::ReportResult(Err). %s\n", text.c_str());
  fflush(stdout);
  if (instance_) {
    pp::Var var_result(fname + "\n" + text);
    instance_->PostMessage(var_result);
  }
}

