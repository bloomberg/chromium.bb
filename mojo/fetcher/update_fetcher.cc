// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/fetcher/update_fetcher.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"

namespace mojo {
namespace fetcher {

namespace {
void IgnoreResult(bool result) {
}

}  // namespace
UpdateFetcher::UpdateFetcher(const GURL& url,
                             updater::Updater* updater,
                             const FetchCallback& loader_callback)
    : Fetcher(loader_callback), url_(url), weak_ptr_factory_(this) {
  DVLOG(1) << "updating: " << url_;
  updater->GetPathForApp(
      url.spec(),
      base::Bind(&UpdateFetcher::OnGetAppPath, weak_ptr_factory_.GetWeakPtr()));
}

UpdateFetcher::~UpdateFetcher() {
}

const GURL& UpdateFetcher::GetURL() const {
  return url_;
}

GURL UpdateFetcher::GetRedirectURL() const {
  return GURL::EmptyGURL();
}

GURL UpdateFetcher::GetRedirectReferer() const {
  return GURL::EmptyGURL();
}
URLResponsePtr UpdateFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                            uint32_t skip) {
  URLResponsePtr response(URLResponse::New());
  response->url = String::From(url_);
  DataPipe data_pipe;
  response->body = data_pipe.consumer_handle.Pass();
  int64 file_size;
  if (base::GetFileSize(path_, &file_size)) {
    response->headers = Array<HttpHeaderPtr>(1);
    HttpHeaderPtr header = HttpHeader::New();
    header->name = "Content-Length";
    header->value = base::StringPrintf("%" PRId64, file_size);
    response->headers[0] = header.Pass();
  }
  common::CopyFromFile(path_, data_pipe.producer_handle.Pass(), skip,
                       task_runner, base::Bind(&IgnoreResult));
  return response.Pass();
}

void UpdateFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, path_, base::PathExists(path_)));
}

std::string UpdateFetcher::MimeType() {
  return "";
}

bool UpdateFetcher::HasMojoMagic() {
  std::string magic;
  ReadFileToString(path_, &magic, strlen(kMojoMagic));
  return magic == kMojoMagic;
}

bool UpdateFetcher::PeekFirstLine(std::string* line) {
  std::string start_of_file;
  ReadFileToString(path_, &start_of_file, kMaxShebangLength);
  size_t return_position = start_of_file.find('\n');
  if (return_position == std::string::npos)
    return false;
  *line = start_of_file.substr(0, return_position + 1);
  return true;
}

void UpdateFetcher::OnGetAppPath(const mojo::String& path) {
  path_ = base::FilePath::FromUTF8Unsafe(path);
  loader_callback_.Run(make_scoped_ptr(this));
}

}  // namespace fetcher
}  // namespace mojo
