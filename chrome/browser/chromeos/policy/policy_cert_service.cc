// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace policy {

PolicyCertService::~PolicyCertService() {
  DCHECK(cert_verifier_)
      << "CreatePolicyCertVerifier() must be called after construction.";
}

PolicyCertService::PolicyCertService(
    UserNetworkConfigurationUpdater* net_conf_updater,
    PrefService* user_prefs)
    : cert_verifier_(NULL),
      net_conf_updater_(net_conf_updater),
      user_prefs_(user_prefs),
      weak_ptr_factory_(this) {
  DCHECK(net_conf_updater_);
  DCHECK(user_prefs_);
}

scoped_ptr<PolicyCertVerifier> PolicyCertService::CreatePolicyCertVerifier() {
  base::Closure callback =
      base::Bind(&PolicyCertService::SetUsedPolicyCertificatesOnce,
                 weak_ptr_factory_.GetWeakPtr());
  cert_verifier_ = new PolicyCertVerifier(
      base::Bind(base::IgnoreResult(&content::BrowserThread::PostTask),
                 content::BrowserThread::UI,
                 FROM_HERE,
                 callback));
  // Certs are forwarded to |cert_verifier_|, thus register here after
  // |cert_verifier_| is created.
  net_conf_updater_->AddTrustedCertsObserver(this);

  // Set the current list of trust anchors.
  net::CertificateList trust_anchors;
  net_conf_updater_->GetWebTrustedCertificates(&trust_anchors);
  OnTrustAnchorsChanged(trust_anchors);

  return make_scoped_ptr(cert_verifier_);
}

void PolicyCertService::OnTrustAnchorsChanged(
    const net::CertificateList& trust_anchors) {
  DCHECK(cert_verifier_);
  // It's safe to use base::Unretained here, because it's guaranteed that
  // |cert_verifier_| outlives this object (see description of
  // CreatePolicyCertVerifier).
  // Note: ProfileIOData, which owns the CertVerifier is deleted by a
  // DeleteSoon on IO, i.e. after all pending tasks on IO are finished.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PolicyCertVerifier::SetTrustAnchors,
                 base::Unretained(cert_verifier_),
                 trust_anchors));
}

bool PolicyCertService::UsedPolicyCertificates() const {
  return user_prefs_->GetBoolean(prefs::kUsedPolicyCertificatesOnce);
}

void PolicyCertService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  net_conf_updater_->RemoveTrustedCertsObserver(this);
  OnTrustAnchorsChanged(net::CertificateList());
  net_conf_updater_ = NULL;
  user_prefs_ = NULL;
}

void PolicyCertService::SetUsedPolicyCertificatesOnce() {
  user_prefs_->SetBoolean(prefs::kUsedPolicyCertificatesOnce, true);
}

}  // namespace policy
