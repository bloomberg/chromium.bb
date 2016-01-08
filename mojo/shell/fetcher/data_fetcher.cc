// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/data_fetcher.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/data_url.h"

namespace mojo {
namespace shell {

ScopedDataPipeConsumerHandle CreateConsumerHandleForString(
    const std::string& data) {
  if (data.size() > std::numeric_limits<uint32_t>::max())
    return ScopedDataPipeConsumerHandle();
  uint32_t num_bytes = static_cast<uint32_t>(data.size());
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = num_bytes;
  mojo::DataPipe data_pipe(options);
  MojoResult result =
      WriteDataRaw(data_pipe.producer_handle.get(), data.data(), &num_bytes,
                   MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  CHECK_EQ(MOJO_RESULT_OK, result);
  return std::move(data_pipe.consumer_handle);
}

// static
void DataFetcher::Start(const GURL& url, const FetchCallback& loader_callback) {
  // The object manages its own lifespan.
  new DataFetcher(url, loader_callback);
}

DataFetcher::DataFetcher(const GURL& url, const FetchCallback& loader_callback)
    : Fetcher(loader_callback), url_(url) {
  BuildAndDispatchResponse();
}

DataFetcher::~DataFetcher() {}

void DataFetcher::BuildAndDispatchResponse() {
  response_ = URLResponse::New();
  response_->url = url_.spec();

  response_->status_code = 400;  // Bad request
  if (url_.SchemeIs(url::kDataScheme)) {
    std::string mime_type, charset, data;
    if (net::DataURL::Parse(url_, &mime_type, &charset, &data)) {
      response_->status_code = 200;
      response_->mime_type = mime_type;
      response_->charset = charset;
      if (!data.empty())
        response_->body = CreateConsumerHandleForString(data);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(loader_callback_,
                            base::Passed(make_scoped_ptr<Fetcher>(this))));
}

const GURL& DataFetcher::GetURL() const {
  return url_;
}

GURL DataFetcher::GetRedirectURL() const {
  return GURL::EmptyGURL();
}

GURL DataFetcher::GetRedirectReferer() const {
  return GURL::EmptyGURL();
}

URLResponsePtr DataFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                          uint32_t skip) {
  DCHECK(response_);
  return std::move(response_);
}

void DataFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::FilePath(), false));
}

std::string DataFetcher::MimeType() {
  DCHECK(response_);
  return response_->mime_type;
}

bool DataFetcher::HasMojoMagic() {
  return false;
}

bool DataFetcher::PeekFirstLine(std::string* line) {
  // This is only called for 'mojo magic' (i.e. detecting shebang'ed
  // content-handler. Since HasMojoMagic() returns false above, this should
  // never be reached.
  NOTREACHED();
  return false;
}

}  // namespace shell
}  // namespace mojo
