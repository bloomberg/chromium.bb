// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_fraudulent_certificate_reporter.h"

#include <set>

#include "base/base64.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chrome/browser/net/cert_logger.pb.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

ChromeFraudulentCertificateReporter::ChromeFraudulentCertificateReporter(
    net::URLRequestContext* request_context)
    : request_context_(request_context),
      upload_url_(FRAUDULENT_CERTIFICATE_UPLOAD_ENDPOINT) {
}

ChromeFraudulentCertificateReporter::~ChromeFraudulentCertificateReporter() {
  STLDeleteElements(&inflight_requests_);
}

// TODO(palmer): Move this to some globally-visible utility module.
static bool DerToPem(const std::string& der_certificate, std::string* output) {
  std::string b64_encoded;
  if (!base::Base64Encode(der_certificate, &b64_encoded))
    return false;

  *output = "-----BEGIN CERTIFICATE-----\r\n";

  size_t size = b64_encoded.size();
  for (size_t i = 0; i < size; ) {
    size_t todo = size - i;
    if (todo > 64)
      todo = 64;
    *output += b64_encoded.substr(i, todo);
    *output += "\r\n";
    i += todo;
  }

  *output += "-----END CERTIFICATE-----\r\n";
  return true;
}

static std::string BuildReport(
    const std::string& hostname,
    const net::SSLInfo& ssl_info) {
  CertLoggerRequest request;
  base::Time now = base::Time::Now();
  request.set_time_usec(now.ToInternalValue());
  request.set_hostname(hostname);

  std::string der_encoded, pem_encoded;

  net::X509Certificate* certificate = ssl_info.cert;
  if (!certificate->GetDEREncoded(&der_encoded) ||
      !DerToPem(der_encoded, &pem_encoded)) {
    LOG(ERROR) << "Could not PEM encode DER certificate";
  }

  std::string* cert_chain = request.mutable_cert_chain();
  *cert_chain += pem_encoded;

  const net::X509Certificate::OSCertHandles& intermediates =
      certificate->GetIntermediateCertificates();

  for (net::X509Certificate::OSCertHandles::const_iterator
      i = intermediates.begin(); i != intermediates.end(); ++i) {
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromHandle(*i, intermediates);

    if (!cert->GetDEREncoded(&der_encoded) ||
        !DerToPem(der_encoded, &pem_encoded)) {
      LOG(ERROR) << "Could not PEM encode DER certificate";
      continue;
    }

    *cert_chain += pem_encoded;
  }

  std::string out;
  request.SerializeToString(&out);
  return out;
}

net::URLRequest* ChromeFraudulentCertificateReporter::CreateURLRequest() {
  return new net::URLRequest(upload_url_, this);
}

void ChromeFraudulentCertificateReporter::SendReport(
    const std::string& hostname,
    const net::SSLInfo& ssl_info,
    bool sni_available) {
  // We do silent/automatic reporting ONLY for Google properties. For other
  // domains (when we start supporting that), we will ask for user permission.
  if (!net::TransportSecurityState::IsGooglePinnedProperty(hostname,
                                                           sni_available)) {
    return;
  }

  std::string report = BuildReport(hostname, ssl_info);

  net::URLRequest* url_request = CreateURLRequest();
  url_request->set_context(request_context_);
  url_request->set_method("POST");
  url_request->AppendBytesToUpload(report.data(), report.size());

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kContentType,
                    "x-application/chrome-fraudulent-cert-report");
  url_request->SetExtraRequestHeaders(headers);

  inflight_requests_.insert(url_request);
  url_request->Start();
}

void ChromeFraudulentCertificateReporter::RequestComplete(
    net::URLRequest* request) {
  std::set<net::URLRequest*>::iterator i = inflight_requests_.find(request);
  DCHECK(i != inflight_requests_.end());
  delete *i;
  inflight_requests_.erase(i);
}

// TODO(palmer): Currently, the upload is fire-and-forget but soon we will
// try to recover by retrying, and trying different endpoints, and
// appealing to the user.
void ChromeFraudulentCertificateReporter::OnResponseStarted(
    net::URLRequest* request) {
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

void ChromeFraudulentCertificateReporter::OnReadCompleted(
    net::URLRequest* request, int bytes_read) {}

}  // namespace chrome_browser_net

