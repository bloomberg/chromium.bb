// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_certificate_generator.h"

#include <utility>

#include "base/macros.h"
#include "content/renderer/media/peer_connection_identity_store.h"
#include "content/renderer/media/rtc_certificate.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/render_thread_impl.h"
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
  RTCCertificateIdentityObserver(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread)
      : main_thread_(main_thread),
        signaling_thread_(signaling_thread),
        observer_(nullptr) {
    DCHECK(main_thread_);
    DCHECK(signaling_thread_);
  }
  ~RTCCertificateIdentityObserver() override {}

  // Perform |store|->RequestIdentity with this identity observer and ensure
  // that this identity observer is not deleted until the request has completed
  // by holding on to a reference to itself for the duration of the request.
  void RequestIdentity(
      const blink::WebRTCKeyParams& key_params,
      const GURL& url,
      const GURL& first_party_for_cookies,
      blink::WebCallbacks<blink::WebRTCCertificate*, void>* observer) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!observer_) << "Already have a RequestIdentity in progress.";
    DCHECK(observer);
    key_params_ = key_params;
    observer_ = observer;
    // Identity request must be performed on the WebRTC signaling thread.
    signaling_thread_->PostTask(FROM_HERE, base::Bind(
        &RTCCertificateIdentityObserver::RequestIdentityOnWebRtcSignalingThread,
        this, url, first_party_for_cookies));
  }

 private:
  void RequestIdentityOnWebRtcSignalingThread(
      GURL url,
      GURL first_party_for_cookies) {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    rtc::scoped_ptr<PeerConnectionIdentityStore> store(
        new PeerConnectionIdentityStore(
            main_thread_, signaling_thread_, url, first_party_for_cookies));
    // Request identity with |this| as the observer. OnSuccess/OnFailure will be
    // called asynchronously.
    store->RequestIdentity(WebRTCKeyParamsToKeyParams(key_params_), this);
  }

  // webrtc::DtlsIdentityRequestObserver implementation.
  void OnFailure(int error) override {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    DCHECK(observer_);
    main_thread_->PostTask(FROM_HERE, base::Bind(
        &RTCCertificateIdentityObserver::DoCallbackOnMainThread,
        this, nullptr));
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
    OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity>(
        rtc::SSLIdentity::FromPEMStrings(pem_key, pem_cert)));
  }
  void OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity> identity) override {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    DCHECK(observer_);
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::Create(std::move(identity));
    main_thread_->PostTask(FROM_HERE, base::Bind(
        &RTCCertificateIdentityObserver::DoCallbackOnMainThread,
        this, new RTCCertificate(key_params_, certificate)));
  }

  void DoCallbackOnMainThread(blink::WebRTCCertificate* certificate) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(observer_);
    if (certificate)
      observer_->onSuccess(certificate);
    else
      observer_->onError();
  }

  // The main thread is the renderer thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // The signaling thread is a WebRTC thread used to invoke
  // PeerConnectionIdentityStore::RequestIdentity on, as is required.
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
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
  DCHECK(isSupportedKeyParams(key_params));

#if defined(ENABLE_WEBRTC)
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread =
      base::ThreadTaskRunnerHandle::Get();

  PeerConnectionDependencyFactory* pc_dependency_factory =
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
  pc_dependency_factory->EnsureInitialized();
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread =
      pc_dependency_factory->GetWebRtcSignalingThread();

  rtc::scoped_refptr<RTCCertificateIdentityObserver> identity_observer(
      new rtc::RefCountedObject<RTCCertificateIdentityObserver>(
          main_thread, signaling_thread));
  // |identity_observer| lives until request has completed.
  identity_observer->RequestIdentity(
      key_params, url, first_party_for_cookies, observer);
#else
  observer->onError();
#endif
}

bool RTCCertificateGenerator::isSupportedKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  return WebRTCKeyParamsToKeyParams(key_params).IsValid();
}

}  // namespace content
