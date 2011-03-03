// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_http_job.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_filter.h"

static const char kMockHostname[] = "mock.http";
static const FilePath::CharType kMockHeaderFileSuffix[] =
    FILE_PATH_LITERAL(".mock-http-headers");

FilePath URLRequestMockHTTPJob::base_path_;

// static
net::URLRequestJob* URLRequestMockHTTPJob::Factory(net::URLRequest* request,
                                                   const std::string& scheme) {
  return new URLRequestMockHTTPJob(request,
                                   GetOnDiskPath(base_path_, request, scheme));
}

// static
void URLRequestMockHTTPJob::AddUrlHandler(const FilePath& base_path) {
  base_path_ = base_path;

  // Add kMockHostname to net::URLRequestFilter.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameHandler("http", kMockHostname,
                             URLRequestMockHTTPJob::Factory);
}

/* static */
GURL URLRequestMockHTTPJob::GetMockUrl(const FilePath& path) {
  std::string url = "http://";
  url.append(kMockHostname);
  url.append("/");
  std::string path_str = path.MaybeAsASCII();
  DCHECK(!path_str.empty());  // We only expect ASCII paths in tests.
  url.append(path_str);
  return GURL(url);
}

/* static */
GURL URLRequestMockHTTPJob::GetMockViewSourceUrl(const FilePath& path) {
  std::string url = chrome::kViewSourceScheme;
  url.append(":");
  url.append(GetMockUrl(path).spec());
  return GURL(url);
}

/* static */
FilePath URLRequestMockHTTPJob::GetOnDiskPath(const FilePath& base_path,
                                              net::URLRequest* request,
                                              const std::string& scheme) {
  // Conceptually we just want to "return base_path + request->url().path()".
  // But path in the request URL is in URL space (i.e. %-encoded spaces).
  // So first we convert base FilePath to a URL, then append the URL
  // path to that, and convert the final URL back to a FilePath.
  GURL file_url(net::FilePathToFileURL(base_path));
  std::string url = file_url.spec() + request->url().path();
  FilePath file_path;
  net::FileURLToFilePath(GURL(url), &file_path);
  return file_path;
}

URLRequestMockHTTPJob::URLRequestMockHTTPJob(net::URLRequest* request,
                                             const FilePath& file_path)
    : net::URLRequestFileJob(request, file_path) { }

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

  FilePath header_file = FilePath(file_path_.value() + kMockHeaderFileSuffix);
  std::string raw_headers;
  if (!file_util::ReadFileToString(header_file, &raw_headers))
    return;

  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\n", std::string("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

bool URLRequestMockHTTPJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers && info.headers->GetMimeType(mime_type);
}

bool URLRequestMockHTTPJob::GetCharset(std::string* charset) {
  net::HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers && info.headers->GetCharset(charset);
}
