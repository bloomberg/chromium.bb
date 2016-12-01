// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "net/base/net_errors.h"
#include "net/ssl/client_key_store.h"

namespace chromeos {

namespace {

void PostSignResultToTaskRunner(
    const scoped_refptr<base::TaskRunner>& target_task_runner,
    const net::SSLPrivateKey::SignCallback& callback,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  target_task_runner->PostTask(FROM_HERE,
                               base::Bind(callback, error, signature));
}

void PostCertificatesToTaskRunner(
    const scoped_refptr<base::TaskRunner>& target_task_runner,
    const base::Callback<void(const net::CertificateList&)>& callback,
    const net::CertificateList& certs) {
  target_task_runner->PostTask(FROM_HERE, base::Bind(callback, certs));
}

}  // namespace

class CertificateProviderService::CertKeyProviderImpl
    : public net::ClientKeyStore::CertKeyProvider {
 public:
  // |certificate_map| must outlive this provider. |service| must be
  // dereferenceable on |service_task_runner|.
  // This provider may be accessed from any thread. Methods and destructor must
  // never be called concurrently.
  CertKeyProviderImpl(
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
      const base::WeakPtr<CertificateProviderService>& service,
      certificate_provider::ThreadSafeCertificateMap* certificate_map);
  ~CertKeyProviderImpl() override;

  bool GetCertificateKey(
      const net::X509Certificate& cert,
      scoped_refptr<net::SSLPrivateKey>* private_key) override;

 private:
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  // Must be dereferenced on |service_task_runner_| only.
  base::WeakPtr<CertificateProviderService> service_;
  certificate_provider::ThreadSafeCertificateMap* const certificate_map_;

  DISALLOW_COPY_AND_ASSIGN(CertKeyProviderImpl);
};

class CertificateProviderService::CertificateProviderImpl
    : public CertificateProvider {
 public:
  // Any calls back to |service| will be posted to |service_task_runner|.
  // |service| must be dereferenceable on |service_task_runner|.
  // This provider is not thread safe, but can be used on any thread.
  CertificateProviderImpl(
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
      const base::WeakPtr<CertificateProviderService>& service);
  ~CertificateProviderImpl() override;

  void GetCertificates(const base::Callback<void(const net::CertificateList&)>&
                           callback) override;

  std::unique_ptr<CertificateProvider> Copy() override;

 private:
  static void GetCertificatesOnServiceThread(
      const base::WeakPtr<CertificateProviderService>& service,
      const base::Callback<void(const net::CertificateList&)>& callback);

  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  // Must be dereferenced on |service_task_runner_| only.
  const base::WeakPtr<CertificateProviderService> service_;

  DISALLOW_COPY_AND_ASSIGN(CertificateProviderImpl);
};

// Implements an SSLPrivateKey backed by the signing function exposed by an
// extension through the certificateProvider API.
// Objects of this class must be used on a single thread. Any thread is allowed.
class CertificateProviderService::SSLPrivateKey : public net::SSLPrivateKey {
 public:
  // Any calls back to |service| will be posted to |service_task_runner|.
  // |service| must be dereferenceable on |service_task_runner|.
  SSLPrivateKey(
      const std::string& extension_id,
      const CertificateInfo& cert_info,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
      const base::WeakPtr<CertificateProviderService>& service);

  // net::SSLPrivateKey:
  Type GetType() override;
  std::vector<net::SSLPrivateKey::Hash> GetDigestPreferences() override;
  size_t GetMaxSignatureLengthInBytes() override;
  void SignDigest(Hash hash,
                  const base::StringPiece& input,
                  const SignCallback& callback) override;

 private:
  ~SSLPrivateKey() override;

  static void SignDigestOnServiceTaskRunner(
      const base::WeakPtr<CertificateProviderService>& service,
      const std::string& extension_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      Hash hash,
      const std::string& input,
      const SignCallback& callback);

  void DidSignDigest(const SignCallback& callback,
                     net::Error error,
                     const std::vector<uint8_t>& signature);

  const std::string extension_id_;
  const CertificateInfo cert_info_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  // Must be dereferenced on |service_task_runner_| only.
  const base::WeakPtr<CertificateProviderService> service_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<SSLPrivateKey> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSLPrivateKey);
};

CertificateProviderService::CertKeyProviderImpl::CertKeyProviderImpl(
    const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
    const base::WeakPtr<CertificateProviderService>& service,
    certificate_provider::ThreadSafeCertificateMap* certificate_map)
    : service_task_runner_(service_task_runner),
      service_(service),
      certificate_map_(certificate_map) {}

CertificateProviderService::CertKeyProviderImpl::~CertKeyProviderImpl() {}

bool CertificateProviderService::CertKeyProviderImpl::GetCertificateKey(
    const net::X509Certificate& cert,
    scoped_refptr<net::SSLPrivateKey>* private_key) {
  bool is_currently_provided = false;
  CertificateInfo info;
  std::string extension_id;
  certificate_map_->LookUpCertificate(cert, &is_currently_provided, &info,
                                      &extension_id);
  if (!is_currently_provided)
    return false;

  *private_key =
      new SSLPrivateKey(extension_id, info, service_task_runner_, service_);

  return true;
}

CertificateProviderService::CertificateProviderImpl::CertificateProviderImpl(
    const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
    const base::WeakPtr<CertificateProviderService>& service)
    : service_task_runner_(service_task_runner), service_(service) {}

CertificateProviderService::CertificateProviderImpl::
    ~CertificateProviderImpl() {}

void CertificateProviderService::CertificateProviderImpl::GetCertificates(
    const base::Callback<void(const net::CertificateList&)>& callback) {
  const scoped_refptr<base::TaskRunner> source_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  const base::Callback<void(const net::CertificateList&)>
      callback_from_service_thread = base::Bind(&PostCertificatesToTaskRunner,
                                                source_task_runner, callback);

  service_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GetCertificatesOnServiceThread, service_,
                            callback_from_service_thread));
}

std::unique_ptr<CertificateProvider>
CertificateProviderService::CertificateProviderImpl::Copy() {
  return base::WrapUnique(
      new CertificateProviderImpl(service_task_runner_, service_));
}

// static
void CertificateProviderService::CertificateProviderImpl::
    GetCertificatesOnServiceThread(
        const base::WeakPtr<CertificateProviderService>& service,
        const base::Callback<void(const net::CertificateList&)>& callback) {
  if (!service) {
    callback.Run(net::CertificateList());
    return;
  }
  service->GetCertificatesFromExtensions(callback);
}

CertificateProviderService::SSLPrivateKey::SSLPrivateKey(
    const std::string& extension_id,
    const CertificateInfo& cert_info,
    const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
    const base::WeakPtr<CertificateProviderService>& service)
    : extension_id_(extension_id),
      cert_info_(cert_info),
      service_task_runner_(service_task_runner),
      service_(service),
      weak_factory_(this) {
  // This constructor is called on |service_task_runner|. Only subsequent calls
  // to member functions have to be on a common thread.
  thread_checker_.DetachFromThread();
}

CertificateProviderService::SSLPrivateKey::Type
CertificateProviderService::SSLPrivateKey::GetType() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cert_info_.type;
}

std::vector<net::SSLPrivateKey::Hash>
CertificateProviderService::SSLPrivateKey::GetDigestPreferences() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cert_info_.supported_hashes;
}

size_t
CertificateProviderService::SSLPrivateKey::GetMaxSignatureLengthInBytes() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cert_info_.max_signature_length_in_bytes;
}

// static
void CertificateProviderService::SSLPrivateKey::SignDigestOnServiceTaskRunner(
    const base::WeakPtr<CertificateProviderService>& service,
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    Hash hash,
    const std::string& input,
    const SignCallback& callback) {
  if (!service) {
    const std::vector<uint8_t> no_signature;
    callback.Run(net::ERR_FAILED, no_signature);
    return;
  }
  service->RequestSignatureFromExtension(extension_id, certificate, hash, input,
                                         callback);
}

void CertificateProviderService::SSLPrivateKey::SignDigest(
    Hash hash,
    const base::StringPiece& input,
    const SignCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const scoped_refptr<base::TaskRunner> source_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  const SignCallback bound_callback =
      // The CertificateProviderService calls back on another thread, so post
      // back to the current thread.
      base::Bind(&PostSignResultToTaskRunner, source_task_runner,
                 // Drop the result and don't call back if this key handle is
                 // destroyed in the meantime.
                 base::Bind(&SSLPrivateKey::DidSignDigest,
                            weak_factory_.GetWeakPtr(), callback));

  service_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SSLPrivateKey::SignDigestOnServiceTaskRunner,
                            service_, extension_id_, cert_info_.certificate,
                            hash, input.as_string(), bound_callback));
}

CertificateProviderService::SSLPrivateKey::~SSLPrivateKey() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void CertificateProviderService::SSLPrivateKey::DidSignDigest(
    const SignCallback& callback,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(error, signature);
}

CertificateProviderService::CertificateProviderService()
    : weak_factory_(this) {}

CertificateProviderService::~CertificateProviderService() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // ClientKeyStore serializes access to |cert_key_provider_|.
  // Once RemoveProvider() returns, it is guaranteed that there are no more
  // accesses to |cert_key_provider_| in flight and no references to
  // |cert_key_provider_| are remaining. This service will hold the last
  // reference to |cert_key_provider_|.
  net::ClientKeyStore::GetInstance()->RemoveProvider(cert_key_provider_.get());
  cert_key_provider_.reset();
}

void CertificateProviderService::SetDelegate(
    std::unique_ptr<Delegate> delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!delegate_);
  DCHECK(delegate);

  delegate_ = std::move(delegate);
  cert_key_provider_.reset(
      new CertKeyProviderImpl(base::ThreadTaskRunnerHandle::Get(),
                              weak_factory_.GetWeakPtr(), &certificate_map_));
  net::ClientKeyStore::GetInstance()->AddProvider(cert_key_provider_.get());
}

bool CertificateProviderService::SetCertificatesProvidedByExtension(
    const std::string& extension_id,
    int cert_request_id,
    const CertificateInfoList& certificate_infos) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool completed = false;
  if (!certificate_requests_.SetCertificates(extension_id, cert_request_id,
                                             certificate_infos, &completed)) {
    DLOG(WARNING) << "Unexpected reply of extension " << extension_id
                  << " to request " << cert_request_id;
    return false;
  }
  if (completed) {
    std::map<std::string, CertificateInfoList> certificates;
    base::Callback<void(const net::CertificateList&)> callback;
    certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                        &callback);
    UpdateCertificatesAndRun(certificates, callback);
  }
  return true;
}

void CertificateProviderService::ReplyToSignRequest(
    const std::string& extension_id,
    int sign_request_id,
    const std::vector<uint8_t>& signature) {
  DCHECK(thread_checker_.CalledOnValidThread());

  net::SSLPrivateKey::SignCallback callback;
  if (!sign_requests_.RemoveRequest(extension_id, sign_request_id, &callback)) {
    LOG(ERROR) << "request id unknown.";
    // Maybe multiple replies to the same request.
    return;
  }

  const net::Error error_code = signature.empty() ? net::ERR_FAILED : net::OK;
  callback.Run(error_code, signature);
}

bool CertificateProviderService::LookUpCertificate(
    const net::X509Certificate& cert,
    bool* has_extension,
    std::string* extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CertificateInfo unused_info;
  return certificate_map_.LookUpCertificate(cert, has_extension, &unused_info,
                                            extension_id);
}

std::unique_ptr<CertificateProvider>
CertificateProviderService::CreateCertificateProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return base::MakeUnique<CertificateProviderImpl>(
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());
}

void CertificateProviderService::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const int cert_request_id :
       certificate_requests_.DropExtension(extension_id)) {
    std::map<std::string, CertificateInfoList> certificates;
    base::Callback<void(const net::CertificateList&)> callback;
    certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                        &callback);
    UpdateCertificatesAndRun(certificates, callback);
  }

  certificate_map_.RemoveExtension(extension_id);

  for (auto callback : sign_requests_.RemoveAllRequests(extension_id))
    callback.Run(net::ERR_FAILED, std::vector<uint8_t>());

  pin_dialog_manager_.ExtensionUnloaded(extension_id);
}

void CertificateProviderService::GetCertificatesFromExtensions(
    const base::Callback<void(const net::CertificateList&)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::vector<std::string> provider_extensions(
      delegate_->CertificateProviderExtensions());

  if (provider_extensions.empty()) {
    DVLOG(2) << "No provider extensions left, clear all certificates.";
    UpdateCertificatesAndRun(std::map<std::string, CertificateInfoList>(),
                             callback);
    return;
  }

  const int cert_request_id = certificate_requests_.AddRequest(
      provider_extensions, callback,
      base::Bind(&CertificateProviderService::TerminateCertificateRequest,
                 base::Unretained(this)));

  DVLOG(2) << "Start certificate request " << cert_request_id;
  delegate_->BroadcastCertificateRequest(cert_request_id);
}

void CertificateProviderService::UpdateCertificatesAndRun(
    const std::map<std::string, CertificateInfoList>& extension_to_certificates,
    const base::Callback<void(const net::CertificateList&)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Extensions are removed from the service's state when they're unloaded.
  // Any remaining extension is assumed to be enabled.
  certificate_map_.Update(extension_to_certificates);

  net::CertificateList all_certs;
  for (const auto& entry : extension_to_certificates) {
    for (const CertificateInfo& cert_info : entry.second)
      all_certs.push_back(cert_info.certificate);
  }

  callback.Run(all_certs);
}

void CertificateProviderService::TerminateCertificateRequest(
    int cert_request_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::map<std::string, CertificateInfoList> certificates;
  base::Callback<void(const net::CertificateList&)> callback;
  if (!certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                           &callback)) {
    DLOG(WARNING) << "Request id " << cert_request_id << " unknown.";
    return;
  }

  DVLOG(1) << "Time out certificate request " << cert_request_id;
  UpdateCertificatesAndRun(certificates, callback);
}

void CertificateProviderService::RequestSignatureFromExtension(
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    net::SSLPrivateKey::Hash hash,
    const std::string& digest,
    const net::SSLPrivateKey::SignCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int sign_request_id = sign_requests_.AddRequest(extension_id, callback);
  if (!delegate_->DispatchSignRequestToExtension(extension_id, sign_request_id,
                                                 hash, certificate, digest)) {
    sign_requests_.RemoveRequest(extension_id, sign_request_id,
                                 nullptr /* callback */);
    callback.Run(net::ERR_FAILED, std::vector<uint8_t>());
  }
}

}  // namespace chromeos
