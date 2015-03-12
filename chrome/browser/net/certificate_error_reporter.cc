// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/certificate_error_reporter.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/net/cert_logger.pb.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

CertificateErrorReporter::CertificateErrorReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url)
    : request_context_(request_context), upload_url_(upload_url) {
  DCHECK(!upload_url.is_empty());
}

CertificateErrorReporter::~CertificateErrorReporter() {
  STLDeleteElements(&inflight_requests_);
}

void CertificateErrorReporter::SendReport(ReportType type,
                                          const std::string& hostname,
                                          const net::SSLInfo& ssl_info) {
  CertLoggerRequest request;
  std::string out;

  BuildReport(hostname, ssl_info, &request);

  switch (type) {
    case REPORT_TYPE_PINNING_VIOLATION:
      SendCertLoggerRequest(request);
      break;
    case REPORT_TYPE_EXTENDED_REPORTING:
      // TODO(estark): Double-check that the user is opted in.
      // TODO(estark): Temporarily, since this is no upload endpoint, just
      // log the information.
      request.SerializeToString(&out);
      DVLOG(3) << "SSL report for " << hostname << ":\n" << out << "\n\n";
      break;
    default:
      NOTREACHED();
  }
}

void CertificateErrorReporter::OnResponseStarted(net::URLRequest* request) {
  const net::URLRequestStatus& status(request->status());
  if (!status.is_success()) {
    LOG(WARNING) << "Certificate upload failed"
                 << " status:" << status.status()
                 << " error:" << status.error();
  } else if (request->GetResponseCode() != 200) {
    LOG(WARNING) << "Certificate upload HTTP status: "
                 << request->GetResponseCode();
  }
  RequestComplete(request);
}

void CertificateErrorReporter::OnReadCompleted(net::URLRequest* request,
                                               int bytes_read) {
}

scoped_ptr<net::URLRequest> CertificateErrorReporter::CreateURLRequest(
    net::URLRequestContext* context) {
  scoped_ptr<net::URLRequest> request =
      context->CreateRequest(upload_url_, net::DEFAULT_PRIORITY, this, NULL);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  return request.Pass();
}

void CertificateErrorReporter::SendCertLoggerRequest(
    const CertLoggerRequest& request) {
  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  scoped_ptr<net::URLRequest> url_request = CreateURLRequest(request_context_);
  url_request->set_method("POST");

  scoped_ptr<net::UploadElementReader> reader(
      net::UploadOwnedBytesElementReader::CreateWithString(serialized_request));
  url_request->set_upload(
      net::ElementsUploadDataStream::CreateWithReader(reader.Pass(), 0));

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kContentType,
                    "x-application/chrome-fraudulent-cert-report");
  url_request->SetExtraRequestHeaders(headers);

  net::URLRequest* raw_url_request = url_request.get();
  inflight_requests_.insert(url_request.release());
  raw_url_request->Start();
}

void CertificateErrorReporter::BuildReport(const std::string& hostname,
                                           const net::SSLInfo& ssl_info,
                                           CertLoggerRequest* out_request) {
  base::Time now = base::Time::Now();
  out_request->set_time_usec(now.ToInternalValue());
  out_request->set_hostname(hostname);

  std::vector<std::string> pem_encoded_chain;
  if (!ssl_info.cert->GetPEMEncodedChain(&pem_encoded_chain))
    LOG(ERROR) << "Could not get PEM encoded chain.";

  std::string* cert_chain = out_request->mutable_cert_chain();
  for (size_t i = 0; i < pem_encoded_chain.size(); ++i)
    *cert_chain += pem_encoded_chain[i];

  out_request->add_pin(ssl_info.pinning_failure_log);
}

void CertificateErrorReporter::RequestComplete(net::URLRequest* request) {
  std::set<net::URLRequest*>::iterator i = inflight_requests_.find(request);
  DCHECK(i != inflight_requests_.end());
  scoped_ptr<net::URLRequest> url_request(*i);
  inflight_requests_.erase(i);
}

}  // namespace chrome_browser_net
