// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/multi_threaded_cert_verifier.h"

namespace policy {

namespace {

void TaintProfile(void* profile_ptr) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_ptr);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;
  profile->GetPrefs()->SetBoolean(prefs::kUsedPolicyCertificatesOnce, true);
}

void MaybeTaintProfile(const net::CertVerifyResult& verify_result,
                       void* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (verify_result.is_issued_by_additional_trust_anchor) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(&TaintProfile, profile));
  }
}

void CallbackWrapper(void* profile,
                     const net::CertVerifyResult* verify_result,
                     const net::CompletionCallback& original_callback,
                     int error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (error == net::OK)
    MaybeTaintProfile(*verify_result, profile);
  if (!original_callback.is_null())
    original_callback.Run(error);
}

}  // namespace

PolicyCertVerifier::PolicyCertVerifier(
    void* profile,
    net::CertTrustAnchorProvider* trust_anchor_provider)
    : profile_(profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::MultiThreadedCertVerifier* verifier =
      new net::MultiThreadedCertVerifier(net::CertVerifyProc::CreateDefault());
  verifier->SetCertTrustAnchorProvider(trust_anchor_provider);
  delegate_.reset(verifier);
}

PolicyCertVerifier::~PolicyCertVerifier() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

int PolicyCertVerifier::Verify(net::X509Certificate* cert,
                               const std::string& hostname,
                               int flags,
                               net::CRLSet* crl_set,
                               net::CertVerifyResult* verify_result,
                               const net::CompletionCallback& callback,
                               RequestHandle* out_req,
                               const net::BoundNetLog& net_log) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::CompletionCallback wrapped_callback =
      base::Bind(&CallbackWrapper, profile_, verify_result, callback);
  int error = delegate_->Verify(cert, hostname, flags, crl_set, verify_result,
                                wrapped_callback, out_req, net_log);
  if (error == net::OK)
    MaybeTaintProfile(*verify_result, profile_);
  return error;
}

void PolicyCertVerifier::CancelRequest(RequestHandle req) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  delegate_->CancelRequest(req);
}

}  // namespace policy
