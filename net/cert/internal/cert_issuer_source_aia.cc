// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/cert_issuer_source_aia.h"

#include "base/strings/string_piece.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/pem.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

namespace net {

namespace {

// TODO(mattm): These are arbitrary choices. Re-evaluate.
const int kTimeoutMilliseconds = 10000;
const int kMaxResponseBytes = 65536;
const int kMaxFetchesPerCert = 5;

bool ParseCertFromDer(const uint8_t* data,
                      size_t length,
                      ParsedCertificateList* results) {
  CertErrors errors;
  if (!ParsedCertificate::CreateAndAddToVector(
          x509_util::CreateCryptoBuffer(data, length),
          x509_util::DefaultParseCertificateOptions(), results, &errors)) {
    // TODO(crbug.com/634443): propagate error info.
    LOG(ERROR) << "Error parsing cert retrieved from AIA (as DER):\n"
               << errors.ToDebugString();

    return false;
  }

  return true;
}

bool ParseCertFromPem(const uint8_t* data,
                      size_t length,
                      ParsedCertificateList* results) {
  base::StringPiece data_strpiece(reinterpret_cast<const char*>(data), length);

  PEMTokenizer pem_tokenizer(data_strpiece, {"CERTIFICATE"});
  if (!pem_tokenizer.GetNext())
    return false;

  return ParseCertFromDer(
      reinterpret_cast<const uint8_t*>(pem_tokenizer.data().data()),
      pem_tokenizer.data().size(), results);
}

class AiaRequest : public CertIssuerSource::Request {
 public:
  AiaRequest() = default;
  ~AiaRequest() override;

  // CertIssuerSource::Request implementation.
  void GetNext(ParsedCertificateList* issuers) override;

  void AddCertFetcherRequest(
      std::unique_ptr<CertNetFetcher::Request> cert_fetcher_request);

  bool AddCompletedFetchToResults(Error error,
                                  std::vector<uint8_t> fetched_bytes,
                                  ParsedCertificateList* results);

 private:
  std::vector<std::unique_ptr<CertNetFetcher::Request>> cert_fetcher_requests_;
  size_t current_request_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AiaRequest);
};

AiaRequest::~AiaRequest() = default;

void AiaRequest::GetNext(ParsedCertificateList* out_certs) {
  // TODO(eroman): Rather than blocking in FIFO order, select the one that
  // completes first.
  while (current_request_ < cert_fetcher_requests_.size()) {
    Error error;
    std::vector<uint8_t> bytes;
    auto req = std::move(cert_fetcher_requests_[current_request_++]);
    req->WaitForResult(&error, &bytes);

    if (AddCompletedFetchToResults(error, std::move(bytes), out_certs))
      return;
  }
}

void AiaRequest::AddCertFetcherRequest(
    std::unique_ptr<CertNetFetcher::Request> cert_fetcher_request) {
  DCHECK(cert_fetcher_request);
  cert_fetcher_requests_.push_back(std::move(cert_fetcher_request));
}

bool AiaRequest::AddCompletedFetchToResults(Error error,
                                            std::vector<uint8_t> fetched_bytes,
                                            ParsedCertificateList* results) {
  if (error != OK) {
    // TODO(mattm): propagate error info.
    LOG(ERROR) << "AiaRequest::OnFetchCompleted got error " << error;
    return false;
  }

  // RFC 5280 section 4.2.2.1:
  //
  //    Conforming applications that support HTTP or FTP for accessing
  //    certificates MUST be able to accept individual DER encoded
  //    certificates and SHOULD be able to accept "certs-only" CMS messages.
  //
  // TODO(mattm): Is supporting CMS message format important?

  // TODO(https://crbug.com/870359): Some AIA responses are served as PEM, which
  // is not part of RFC 5280's profile.
  return ParseCertFromDer(fetched_bytes.data(), fetched_bytes.size(),
                          results) ||
         ParseCertFromPem(fetched_bytes.data(), fetched_bytes.size(), results);
}

}  // namespace

CertIssuerSourceAia::CertIssuerSourceAia(
    scoped_refptr<CertNetFetcher> cert_fetcher)
    : cert_fetcher_(std::move(cert_fetcher)) {}

CertIssuerSourceAia::~CertIssuerSourceAia() = default;

void CertIssuerSourceAia::SyncGetIssuersOf(const ParsedCertificate* cert,
                                           ParsedCertificateList* issuers) {
  // CertIssuerSourceAia never returns synchronous results.
}

void CertIssuerSourceAia::AsyncGetIssuersOf(const ParsedCertificate* cert,
                                            std::unique_ptr<Request>* out_req) {
  out_req->reset();

  if (!cert->has_authority_info_access())
    return;

  // RFC 5280 section 4.2.2.1:
  //
  //    An authorityInfoAccess extension may include multiple instances of
  //    the id-ad-caIssuers accessMethod.  The different instances may
  //    specify different methods for accessing the same information or may
  //    point to different information.

  std::vector<GURL> urls;
  for (const auto& uri : cert->ca_issuers_uris()) {
    GURL url(uri);
    if (url.is_valid()) {
      // TODO(mattm): do the kMaxFetchesPerCert check only on the number of
      // supported URL schemes, not all the URLs.
      if (urls.size() < kMaxFetchesPerCert) {
        urls.push_back(url);
      } else {
        // TODO(mattm): propagate error info.
        LOG(ERROR) << "kMaxFetchesPerCert exceeded, skipping";
      }
    } else {
      // TODO(mattm): propagate error info.
      LOG(ERROR) << "invalid AIA URL: " << uri;
    }
  }
  if (urls.empty())
    return;

  std::unique_ptr<AiaRequest> aia_request(new AiaRequest());

  for (const auto& url : urls) {
    // TODO(mattm): add synchronous failure mode to FetchCaIssuers interface so
    // that this doesn't need to wait for async callback just to tell that an
    // URL has an unsupported scheme?
    aia_request->AddCertFetcherRequest(cert_fetcher_->FetchCaIssuers(
        url, kTimeoutMilliseconds, kMaxResponseBytes));
  }

  *out_req = std::move(aia_request);
}

}  // namespace net
