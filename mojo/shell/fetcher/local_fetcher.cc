// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/local_fetcher.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"
#include "url/url_util.h"

namespace mojo {
namespace shell {

namespace {
void IgnoreResult(bool result) {
}

}  // namespace

// A loader for local files.
LocalFetcher::LocalFetcher(NetworkService* network_service,
                           const GURL& url,
                           const GURL& url_without_query,
                           const base::FilePath& shell_file_root,
                           const FetchCallback& loader_callback)
    : Fetcher(loader_callback),
      url_(url),
      path_(util::UrlToFilePath(url_without_query)),
      shell_file_root_(shell_file_root) {
  TRACE_EVENT1("mojo_shell", "LocalFetcher::LocalFetcher", "url", url.spec());
  const std::string ext(base::FilePath(path_.Extension()).AsUTF8Unsafe());
  if (network_service && !base::EqualsCaseInsensitiveASCII(ext, ".mojo")) {
    network_service->GetMimeTypeFromFile(
      path_.AsUTF8Unsafe(),
      base::Bind(&LocalFetcher::GetMimeTypeFromFileCallback,
                 base::Unretained(this)));
  } else {
    loader_callback_.Run(make_scoped_ptr(this));
  }
}

LocalFetcher::~LocalFetcher() {}

void LocalFetcher::GetMimeTypeFromFileCallback(const mojo::String& mime_type) {
  mime_type_ = mime_type.To<std::string>();
  loader_callback_.Run(make_scoped_ptr(this));
}

const GURL& LocalFetcher::GetURL() const {
  return url_;
}

GURL LocalFetcher::GetRedirectURL() const {
  // Use Mandoline's Google Storage bucket if the Mojo component does not exist.
  // TODO(msw): Integrate with Mandoline's component updater? crbug.com/479169
  // TODO(msw): Integrate with planned Omaha fetcher and updater work.
  // TODO(msw): Support fetching apps with resource files.
  // TODO(msw): Handle broken URLs. (unavailable component files, etc.)
  // TODO(msw): Avoid kPredictableAppFilenames switch? (need executable bit set)
  if (!base::PathExists(path_) && shell_file_root_.IsParent(path_) &&
      path_.MatchesExtension(FILE_PATH_LITERAL(".mojo")) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPredictableAppFilenames)) {
    GURL url("https://storage.googleapis.com/mandoline/");
    const std::string version("latest/");
#if defined(OS_WIN)
    const std::string platform("win/");
#elif defined(OS_LINUX)
    const std::string platform("linux/");
#else
    const std::string platform("unknown/");
#endif
    // Get the app path relative to the shell (and Google Storage) file root.
    base::FilePath app_path;
    bool result = shell_file_root_.AppendRelativePath(path_, &app_path);
    DCHECK(result);
    url = url.Resolve(version).Resolve(platform).Resolve(app_path.value());
    return url;
  }
  return GURL::EmptyGURL();
}

GURL LocalFetcher::GetRedirectReferer() const {
  return GURL::EmptyGURL();
}

URLResponsePtr LocalFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                           uint32_t skip) {
  URLResponsePtr response(URLResponse::New());
  response->url = String::From(url_);
  DataPipe data_pipe;
  response->body = std::move(data_pipe.consumer_handle);
  int64_t file_size;
  if (base::GetFileSize(path_, &file_size)) {
    response->headers = Array<HttpHeaderPtr>(1);
    HttpHeaderPtr header = HttpHeader::New();
    header->name = "Content-Length";
    header->value = base::StringPrintf("%" PRId64, file_size);
    response->headers[0] = std::move(header);
  }
  response->mime_type = String::From(MimeType());
  common::CopyFromFile(path_, std::move(data_pipe.producer_handle), skip,
                       task_runner, base::Bind(&IgnoreResult));
  return response;
}

void LocalFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  // Async for consistency with network case.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, path_, base::PathExists(path_)));
}

std::string LocalFetcher::MimeType() {
  return mime_type_;
}

bool LocalFetcher::HasMojoMagic() {
  std::string magic;
  ReadFileToString(path_, &magic, strlen(kMojoMagic));
  return magic == kMojoMagic;
}

bool LocalFetcher::PeekFirstLine(std::string* line) {
  std::string start_of_file;
  ReadFileToString(path_, &start_of_file, kMaxShebangLength);
  size_t return_position = start_of_file.find('\n');
  if (return_position == std::string::npos)
    return false;
  *line = start_of_file.substr(0, return_position + 1);
  return true;
}

}  // namespace shell
}  // namespace mojo
