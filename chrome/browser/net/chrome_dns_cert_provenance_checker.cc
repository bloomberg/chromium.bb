// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_dns_cert_provenance_checker.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "net/url_request/url_request.h"

namespace {

class ChromeDnsCertProvenanceChecker :
    public net::DnsCertProvenanceChecker,
    public net::DnsCertProvenanceChecker::Delegate {
 public:
  ChromeDnsCertProvenanceChecker(
      net::DnsRRResolver* dnsrr_resolver,
      ChromeURLRequestContext* url_req_context)
      : dnsrr_resolver_(dnsrr_resolver),
        url_req_context_(url_req_context),
        upload_url_("http://chromecertcheck.appspot.com/upload"),
        delegate_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  ~ChromeDnsCertProvenanceChecker() {
    DCHECK(inflight_requests_.empty());
  }

  // DnsCertProvenanceChecker interface
  virtual void DoAsyncVerification(
      const std::string& hostname,
      const std::vector<base::StringPiece>& der_certs) {
    net::DnsCertProvenanceChecker::DoAsyncLookup(hostname, der_certs,
                                                 dnsrr_resolver_, this);
  }

  virtual void Shutdown() {
    STLDeleteContainerPointers(inflight_requests_.begin(),
                               inflight_requests_.end());
    inflight_requests_.clear();
  }

  // DnsCertProvenanceChecker::Delegate interface
  virtual void OnDnsCertLookupFailed(
      const std::string& hostname,
      const std::vector<std::string>& der_certs) {
    const std::string report = BuildEncryptedReport(hostname, der_certs);

    net::URLRequest* url_request(new net::URLRequest(upload_url_, &delegate_));
    url_request->set_context(url_req_context_);
    url_request->set_method("POST");
    url_request->AppendBytesToUpload(report.data(), report.size());
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kContentType,
                      "x-application/chrome-cert-provenance-report");
    url_request->SetExtraRequestHeaders(headers);
    inflight_requests_.insert(url_request);
    url_request->Start();
  }

 private:
  void RequestComplete(net::URLRequest* request) {
    std::set<net::URLRequest*>::iterator i = inflight_requests_.find(request);
    DCHECK(i != inflight_requests_.end());
    delete *i;
    inflight_requests_.erase(i);
  }

  // URLRequestDelegate is the delegate for the upload. Since this is a
  // fire-and-forget operation, we don't care if there are any errors in the
  // upload.
  class URLRequestDelegate : public net::URLRequest::Delegate {
   public:
    explicit URLRequestDelegate(ChromeDnsCertProvenanceChecker* checker)
        : checker_(checker) {
    }

    // Delegate implementation
    void OnResponseStarted(net::URLRequest* request) {
      const net::URLRequestStatus& status(request->status());
      if (!status.is_success()) {
        LOG(WARNING) << "Certificate upload failed"
                     << " status:" << status.status()
                     << " os_error:" << status.os_error();
      } else if (request->GetResponseCode() != 200) {
        LOG(WARNING) << "Certificate upload HTTP status: "
                     << request->GetResponseCode();
      }
      checker_->RequestComplete(request);
    }

    void OnReadCompleted(net::URLRequest* request, int bytes_read) {
      NOTREACHED();
    }

   private:
    ChromeDnsCertProvenanceChecker* const checker_;
  };

  net::DnsRRResolver* const dnsrr_resolver_;
  ChromeURLRequestContext* const url_req_context_;
  const GURL upload_url_;
  URLRequestDelegate delegate_;
  std::set<net::URLRequest*> inflight_requests_;
};

}  // namespace

net::DnsCertProvenanceChecker* CreateChromeDnsCertProvenanceChecker(
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context) {
  return new ChromeDnsCertProvenanceChecker(dnsrr_resolver, url_req_context);
}
