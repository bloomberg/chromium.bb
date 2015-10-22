// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_certificate_generator.h"

#include "content/renderer/media/peer_connection_identity_store.h"
#include "content/renderer/media/rtc_certificate.h"
#include "third_party/webrtc/base/rtccertificate.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"
#include "url/gurl.h"

namespace content {
namespace {

rtc::KeyParams WebRTCKeyParamsToKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  switch (key_params.keyType()) {
    case blink::WebRTCKeyTypeRSA:
      return rtc::KeyParams::RSA(key_params.rsaParams().modLength,
                                 key_params.rsaParams().pubExp);
    case blink::WebRTCKeyTypeECDSA:
      return rtc::KeyParams::ECDSA(
          static_cast<rtc::ECCurve>(key_params.ecCurve()));
    default:
      NOTREACHED();
      return rtc::KeyParams();
  }
}

// Observer used by RTCCertificateGenerator::generateCertificate.
class RTCCertificateIdentityObserver
    : public webrtc::DtlsIdentityRequestObserver {
 public:
  RTCCertificateIdentityObserver() : observer_(nullptr) {}
  ~RTCCertificateIdentityObserver() override {}

  // Perform |store|->RequestIdentity with this identity observer and ensure
  // that this identity observer is not deleted until the request has completed
  // by holding on to a reference to itself for the duration of the request.
  void RequestIdentity(
      webrtc::DtlsIdentityStoreInterface* store,
      const blink::WebRTCKeyParams& key_params,
      blink::WebCallbacks<blink::WebRTCCertificate*, void>* observer) {
    DCHECK(!self_ref_) << "Already have a RequestIdentity in progress.";
    self_ref_ = this;
    key_params_ = key_params;
    observer_ = observer;
    DCHECK(observer_);
    // Request identity with |this| as the observer. OnSuccess/OnFailure will be
    // called asynchronously.
    store->RequestIdentity(WebRTCKeyParamsToKeyParams(key_params).type(), this);
  }

 private:
  void OnFailure(int error) override {
    DCHECK(self_ref_) << "Not initialized. See RequestIdentity.";
    DCHECK(observer_);
    observer_->onError();
    // Stop referencing self. If this is the last reference then this will
    // result in "delete this".
    self_ref_ = nullptr;
  }

  void OnSuccess(const std::string& der_cert,
                 const std::string& der_private_key) override {
    std::string pem_cert = rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeCertificate,
        reinterpret_cast<const unsigned char*>(der_cert.data()),
        der_cert.length());
    std::string pem_key = rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeRsaPrivateKey,
        reinterpret_cast<const unsigned char*>(der_private_key.data()),
        der_private_key.length());
    rtc::scoped_ptr<rtc::SSLIdentity> identity(
        rtc::SSLIdentity::FromPEMStrings(pem_key, pem_cert));
    OnSuccess(identity.Pass());
  }

  void OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity> identity) override {
    DCHECK(self_ref_) << "Not initialized. See RequestIdentity.";
    DCHECK(observer_);
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::Create(identity.Pass());
    observer_->onSuccess(new RTCCertificate(key_params_, certificate));
    // Stop referencing self. If this is the last reference then this will
    // result in "delete this".
    self_ref_ = nullptr;
  }

  // The reference to self protects |this| from being deleted before the request
  // has completed. Upon completion we stop referencing ourselves.
  rtc::scoped_refptr<RTCCertificateIdentityObserver> self_ref_;
  blink::WebRTCKeyParams key_params_;
  blink::WebCallbacks<blink::WebRTCCertificate*, void>* observer_;

  DISALLOW_COPY_AND_ASSIGN(RTCCertificateIdentityObserver);
};

}  // namespace

void RTCCertificateGenerator::generateCertificate(
    const blink::WebRTCKeyParams& key_params,
    const blink::WebURL& url,
    const blink::WebURL& first_party_for_cookies,
    blink::WebCallbacks<blink::WebRTCCertificate*, void>* observer) {
  rtc::scoped_ptr<PeerConnectionIdentityStore> store(
      new PeerConnectionIdentityStore(url, first_party_for_cookies));
  rtc::scoped_refptr<RTCCertificateIdentityObserver> identity_observer(
      new rtc::RefCountedObject<RTCCertificateIdentityObserver>());
  // |identity_observer| lives until request has completed.
  identity_observer->RequestIdentity(store.get(), key_params, observer);
}

bool RTCCertificateGenerator::isValidKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  return WebRTCKeyParamsToKeyParams(key_params).IsValid();
}

}  // namespace content
