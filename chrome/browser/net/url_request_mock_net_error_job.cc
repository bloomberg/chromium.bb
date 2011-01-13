// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_net_error_job.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/x509_certificate.h"
#include "net/url_request/url_request_filter.h"

// static
URLRequestMockNetErrorJob::URLMockInfoMap
    URLRequestMockNetErrorJob::url_mock_info_map_;

struct URLRequestMockNetErrorJob::MockInfo {
  MockInfo() : ssl_cert(NULL) { }
  MockInfo(std::wstring base,
           std::vector<int> errors,
           net::X509Certificate* ssl_cert)
      : base(base),
        errors(errors),
        ssl_cert(ssl_cert) { }

  std::wstring base;
  std::vector<int> errors;
  scoped_refptr<net::X509Certificate> ssl_cert;
};

// static
void URLRequestMockNetErrorJob::AddMockedURL(const GURL& url,
                                             const std::wstring& base,
                                             const std::vector<int>& errors,
                                             net::X509Certificate* ssl_cert) {
#ifndef NDEBUG
  URLMockInfoMap::const_iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter == url_mock_info_map_.end());
#endif

  url_mock_info_map_[url] = MockInfo(base, errors, ssl_cert);
  net::URLRequestFilter::GetInstance()
      ->AddUrlHandler(url, &URLRequestMockNetErrorJob::Factory);
}

// static
void URLRequestMockNetErrorJob::RemoveMockedURL(const GURL& url) {
  URLMockInfoMap::iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter != url_mock_info_map_.end());
  url_mock_info_map_.erase(iter);
  net::URLRequestFilter::GetInstance()->RemoveUrlHandler(url);
}

// static
net::URLRequestJob* URLRequestMockNetErrorJob::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  GURL url = request->url();

  URLMockInfoMap::const_iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter != url_mock_info_map_.end());

  MockInfo mock_info = iter->second;

  // URLRequestMockNetErrorJob derives from net::URLRequestFileJob.  We pass a
  // FilePath so that the net::URLRequestFileJob methods will do the loading
  // from the files.
  std::wstring file_url(L"file:///");
  file_url.append(mock_info.base);
  file_url.append(UTF8ToWide(url.path()));
  // Convert the file:/// URL to a path on disk.
  FilePath file_path;
  net::FileURLToFilePath(GURL(WideToUTF8(file_url)), &file_path);
  return new URLRequestMockNetErrorJob(request, mock_info.errors,
                                       mock_info.ssl_cert,
                                       file_path);
}

URLRequestMockNetErrorJob::URLRequestMockNetErrorJob(net::URLRequest* request,
    const std::vector<int>& errors, net::X509Certificate* cert,
    const FilePath& file_path)
    : URLRequestMockHTTPJob(request, file_path),
      errors_(errors),
      ssl_cert_(cert) {
}

URLRequestMockNetErrorJob::~URLRequestMockNetErrorJob() {
}

void URLRequestMockNetErrorJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestMockNetErrorJob::StartAsync));
}

void URLRequestMockNetErrorJob::StartAsync() {
  if (errors_.empty()) {
    URLRequestMockHTTPJob::Start();
  } else {
    int error =  errors_[0];
    errors_.erase(errors_.begin());

    if (net::IsCertificateError(error)) {
      DCHECK(ssl_cert_);
      request_->delegate()->OnSSLCertificateError(request_, error,
                                                  ssl_cert_.get());
    } else {
      NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                             error));
    }
  }
}

void URLRequestMockNetErrorJob::ContinueDespiteLastError() {
  Start();
}
