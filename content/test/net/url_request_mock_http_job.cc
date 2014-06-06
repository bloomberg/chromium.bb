// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/net/url_request_mock_http_job.h"

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

const char kMockHostname[] = "mock.http";
const base::FilePath::CharType kMockHeaderFileSuffix[] =
    FILE_PATH_LITERAL(".mock-http-headers");

namespace content {

namespace {

class MockJobInterceptor : public net::URLRequestInterceptor {
 public:
  // When |map_all_requests_to_base_path| is true, all request should return the
  // contents of the file at |base_path|. When |map_all_requests_to_base_path|
  // is false, |base_path| is the file path leading to the root of the directory
  // to use as the root of the HTTP server.
  MockJobInterceptor(const base::FilePath& base_path,
                     bool map_all_requests_to_base_path)
      : base_path_(base_path),
        map_all_requests_to_base_path_(map_all_requests_to_base_path) {}
  virtual ~MockJobInterceptor() {}

  // net::URLRequestJobFactory::ProtocolHandler implementation
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new URLRequestMockHTTPJob(request, network_delegate,
        map_all_requests_to_base_path_ ? base_path_ : GetOnDiskPath(request));
  }

 private:
  base::FilePath GetOnDiskPath(net::URLRequest* request) const {
    // Conceptually we just want to "return base_path_ + request->url().path()".
    // But path in the request URL is in URL space (i.e. %-encoded spaces).
    // So first we convert base FilePath to a URL, then append the URL
    // path to that, and convert the final URL back to a FilePath.
    GURL file_url(net::FilePathToFileURL(base_path_));
    std::string url = file_url.spec() + request->url().path();
    base::FilePath file_path;
    net::FileURLToFilePath(GURL(url), &file_path);
    return file_path;
  }

  const base::FilePath base_path_;
  const bool map_all_requests_to_base_path_;

  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

}  // namespace

// static
void URLRequestMockHTTPJob::AddUrlHandler(const base::FilePath& base_path) {
  // Add kMockHostname to net::URLRequestFilter.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "http", kMockHostname, CreateInterceptor(base_path));
}

// static
void URLRequestMockHTTPJob::AddHostnameToFileHandler(
    const std::string& hostname,
    const base::FilePath& file) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "http", hostname, CreateInterceptorForSingleFile(file));
}

// static
GURL URLRequestMockHTTPJob::GetMockUrl(const base::FilePath& path) {
  std::string url = "http://";
  url.append(kMockHostname);
  url.append("/");
  std::string path_str = path.MaybeAsASCII();
  DCHECK(!path_str.empty());  // We only expect ASCII paths in tests.
  url.append(path_str);
  return GURL(url);
}

// static
GURL URLRequestMockHTTPJob::GetMockViewSourceUrl(const base::FilePath& path) {
  std::string url = kViewSourceScheme;
  url.append(":");
  url.append(GetMockUrl(path).spec());
  return GURL(url);
}

// static
scoped_ptr<net::URLRequestInterceptor>
URLRequestMockHTTPJob::CreateInterceptor(const base::FilePath& base_path) {
  return scoped_ptr<net::URLRequestInterceptor>(
      new MockJobInterceptor(base_path, false));
}

// static
scoped_ptr<net::URLRequestInterceptor>
URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
    const base::FilePath& file) {
  return scoped_ptr<net::URLRequestInterceptor>(
      new MockJobInterceptor(file, true));
}

URLRequestMockHTTPJob::URLRequestMockHTTPJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate,
    const base::FilePath& file_path)
    : net::URLRequestFileJob(
          request, network_delegate, file_path,
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)) {}

URLRequestMockHTTPJob::~URLRequestMockHTTPJob() { }

// Public virtual version.
void URLRequestMockHTTPJob::GetResponseInfo(net::HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

bool URLRequestMockHTTPJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  // Override the net::URLRequestFileJob implementation to invoke the default
  // one based on HttpResponseInfo.
  return net::URLRequestJob::IsRedirectResponse(location, http_status_code);
}

// Private const version.
void URLRequestMockHTTPJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  // We have to load our headers from disk, but we only use this class
  // from tests, so allow these IO operations to happen on any thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  base::FilePath header_file =
      base::FilePath(file_path_.value() + kMockHeaderFileSuffix);
  std::string raw_headers;
  if (!base::PathExists(header_file)) {
    // If there is no mock-http-headers file, fake a 200 OK.
    raw_headers = "HTTP/1.0 200 OK\n";
  } else {
    if (!base::ReadFileToString(header_file, &raw_headers))
      return;
  }

  // Handle CRLF line-endings.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\r\n", "\n");
  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\n", std::string("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

bool URLRequestMockHTTPJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers.get() && info.headers->GetMimeType(mime_type);
}

int URLRequestMockHTTPJob::GetResponseCode() const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  // If we have headers, get the response code from them.
  if (info.headers.get())
    return info.headers->response_code();
  return net::URLRequestJob::GetResponseCode();
}

bool URLRequestMockHTTPJob::GetCharset(std::string* charset) {
  net::HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers.get() && info.headers->GetCharset(charset);
}

}  // namespace content
