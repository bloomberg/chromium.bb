// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chromeos/network/onc/onc_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/cert_trust_anchor_provider.h"

using content::BrowserThread;

namespace policy {

namespace {

// A simple implementation of net::CertTrustAnchorProvider that returns a list
// of certificates that can be set by the owner of this object.
class CrosTrustAnchorProvider : public net::CertTrustAnchorProvider {
 public:
  CrosTrustAnchorProvider()
      : trust_anchors_(new net::CertificateList) {
  }

  virtual ~CrosTrustAnchorProvider() {
  }

  // CertTrustAnchorProvider overrides.
  virtual const net::CertificateList& GetAdditionalTrustAnchors() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    return *trust_anchors_;
  }

  void SetTrustAnchors(scoped_ptr<net::CertificateList> trust_anchors) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    trust_anchors_ = trust_anchors.Pass();
  }

 private:
  scoped_ptr<net::CertificateList> trust_anchors_;

  DISALLOW_COPY_AND_ASSIGN(CrosTrustAnchorProvider);
};

}  // namespace

NetworkConfigurationUpdater::NetworkConfigurationUpdater()
    : allow_trusted_certificates_from_policy_(false),
      cert_trust_provider_(new CrosTrustAnchorProvider()) {
}

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  bool posted = BrowserThread::DeleteSoon(
      BrowserThread::IO, FROM_HERE, cert_trust_provider_);
  if (!posted)
    delete cert_trust_provider_;
}

net::CertTrustAnchorProvider*
NetworkConfigurationUpdater::GetCertTrustAnchorProvider() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cert_trust_provider_;
}

void NetworkConfigurationUpdater::SetAllowTrustedCertsFromPolicy() {
  allow_trusted_certificates_from_policy_ = true;
}

void NetworkConfigurationUpdater::SetTrustAnchors(
    scoped_ptr<net::CertificateList> web_trust_certs) {
  if (allow_trusted_certificates_from_policy_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CrosTrustAnchorProvider::SetTrustAnchors,
                   base::Unretained(static_cast<CrosTrustAnchorProvider*>(
                       cert_trust_provider_)),
                   base::Passed(&web_trust_certs)));
  }
}

}  // namespace policy
