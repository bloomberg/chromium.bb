// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A net::URLRequestJob class that pulls the content and http headers from disk.

#ifndef CONTENT_TEST_NET_URL_REQUEST_MOCK_HTTP_JOB_H_
#define CONTENT_TEST_NET_URL_REQUEST_MOCK_HTTP_JOB_H_

#include <string>

#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job_factory.h"

class FilePath;

namespace content {

class URLRequestMockHTTPJob : public net::URLRequestFileJob {
 public:
  URLRequestMockHTTPJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        const FilePath& file_path);

  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;

  // Adds the testing URLs to the net::URLRequestFilter.
  static void AddUrlHandler(const FilePath& base_path);

  // Respond to all HTTP requests of |hostname| with contents of the file
  // located at |file_path|.
  static void AddHostnameToFileHandler(const std::string& hostname,
                                       const FilePath& file_path);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL.
  static GURL GetMockUrl(const FilePath& path);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL for view source.
  static GURL GetMockViewSourceUrl(const FilePath& path);

  // Returns a net::URLRequestJobFactory::ProtocolHandler that serves
  // URLRequestMockHTTPJob's responding like an HTTP server. |base_path| is the
  // file path leading to the root of the directory to use as the root of the
  // HTTP server.
  static scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
  CreateProtocolHandler(const FilePath& base_path);

 private:
  virtual ~URLRequestMockHTTPJob();

  void GetResponseInfoConst(net::HttpResponseInfo* info) const;
};

}  // namespace content

#endif  // CONTENT_TEST_NET_URL_REQUEST_MOCK_HTTP_JOB_H_
