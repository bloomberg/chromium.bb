// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CERTIFICATE_ERROR_REPORTER_H_
#define CHROME_BROWSER_NET_CERTIFICATE_ERROR_REPORTER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class URLRequestContext;
class SSLInfo;
}

namespace chrome_browser_net {

class EncryptedCertLoggerRequest;

// Provides functionality for sending reports about invalid SSL
// certificate chains to a report collection server.
class CertificateErrorReporter : public net::URLRequest::Delegate {
 public:
  // These represent the types of reports that can be sent.
  enum ReportType {
    // A report of a certificate chain that failed a certificate pinning
    // check.
    REPORT_TYPE_PINNING_VIOLATION,
    // A report for an invalid certificate chain that is being sent for
    // a user who has opted-in to the extended reporting program.
    REPORT_TYPE_EXTENDED_REPORTING
  };

  // Represents whether or not to send cookies along with reports sent
  // to the server.
  enum CookiesPreference { SEND_COOKIES, DO_NOT_SEND_COOKIES };

  // Creates a certificate error reporter that will send certificate
  // error reports to |upload_url|, using |request_context| as the
  // context for the reports. |cookies_preference| controls whether
  // cookies will be sent along with the reports.
  CertificateErrorReporter(net::URLRequestContext* request_context,
                           const GURL& upload_url,
                           CookiesPreference cookies_preference);

  // Allows tests to use a server public key with known private key.
  CertificateErrorReporter(net::URLRequestContext* request_context,
                           const GURL& upload_url,
                           CookiesPreference cookies_preference,
                           const uint8 server_public_key[32],
                           const uint32 server_public_key_version);

  ~CertificateErrorReporter() override;

  // Sends a certificate report to the report collection server. The
  // |serialized_report| is expected to be a serialized protobuf
  // containing information about the hostname, certificate chain, and
  // certificate errors encountered when validating the chain.
  //
  // |SendReport| actually sends the report over the network; callers are
  // responsible for enforcing any preconditions (such as obtaining user
  // opt-in, only sending reports for certain hostnames, checking for
  // incognito mode, etc.).
  //
  // On some platforms (but not all), CertificateErrorReporter can use
  // an HTTP endpoint to send encrypted extended reporting reports. On
  // unsupported platforms, callers must send extended reporting reports
  // over SSL.
  virtual void SendReport(ReportType type,
                          const std::string& serialized_report);

  // net::URLRequest::Delegate
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // Callers can use this method to determine if sending reports over
  // HTTP is supported.
  static bool IsHttpUploadUrlSupported();

  // Used by tests.
  static bool DecryptCertificateErrorReport(
      const uint8 server_private_key[32],
      const EncryptedCertLoggerRequest& encrypted_report,
      std::string* decrypted_serialized_report);

 private:
  // Creates a URLRequest with which to send a certificate report to the
  // server.
  virtual scoped_ptr<net::URLRequest> CreateURLRequest(
      net::URLRequestContext* context);

  // Sends a serialized report (encrypted or not) to the report
  // collection server.
  void SendSerializedRequest(const std::string& serialized_request);

  // Performs post-report cleanup.
  void RequestComplete(net::URLRequest* request);

  net::URLRequestContext* const request_context_;
  const GURL upload_url_;

  // Owns the contained requests.
  std::set<net::URLRequest*> inflight_requests_;

  CookiesPreference cookies_preference_;

  const uint8* server_public_key_;
  const uint32 server_public_key_version_;

  DISALLOW_COPY_AND_ASSIGN(CertificateErrorReporter);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CERTIFICATE_ERROR_REPORTER_H_
