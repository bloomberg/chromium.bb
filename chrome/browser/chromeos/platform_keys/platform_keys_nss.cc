// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

#include <cryptohi.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/net/nss_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {
const char kErrorInternal[] = "Internal Error.";
const char kErrorKeyNotFound[] = "Key not found.";
const char kErrorCertificateNotFound[] = "Certificate could not be found.";
const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";

// The current maximal RSA modulus length that ChromeOS's TPM supports for key
// generation.
const unsigned int kMaxRSAModulusLengthBits = 2048;
}

namespace chromeos {

namespace platform_keys {

namespace {

// Base class to store state that is common to all NSS database operations and
// to provide convenience methods to call back.
// Keeps track of the originating task runner.
class NSSOperationState {
 public:
  NSSOperationState();
  virtual ~NSSOperationState() {}

  // Called if an error occurred during the execution of the NSS operation
  // described by this object.
  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) = 0;

  crypto::ScopedPK11Slot slot_;

  // The task runner on which the NSS operation was called. Any reply must be
  // posted to this runner.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NSSOperationState);
};

typedef base::Callback<void(net::NSSCertDatabase* cert_db)> GetCertDBCallback;

// Used by GetCertDatabaseOnIOThread and called back with the requested
// NSSCertDatabase.
// If |token_id| is not empty, sets |slot_| of |state| accordingly and calls
// |callback| if the database was successfully retrieved.
void DidGetCertDBOnIOThread(const std::string& token_id,
                            const GetCertDBCallback& callback,
                            NSSOperationState* state,
                            net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!cert_db) {
    LOG(ERROR) << "Couldn't get NSSCertDatabase.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  if (!token_id.empty()) {
    if (token_id == kTokenIdUser)
      state->slot_ = cert_db->GetPrivateSlot();
    else if (token_id == kTokenIdSystem)
      state->slot_ = cert_db->GetSystemSlot();

    if (!state->slot_) {
      LOG(ERROR) << "Slot for token id '" << token_id << "' not available.";
      state->OnError(FROM_HERE, kErrorInternal);
      return;
    }
  }

  callback.Run(cert_db);
}

// Retrieves the NSSCertDatabase from |context| and, if |token_id| is not empty,
// the slot for |token_id|.
// Must be called on the IO thread.
void GetCertDatabaseOnIOThread(const std::string& token_id,
                               const GetCertDBCallback& callback,
                               content::ResourceContext* context,
                               NSSOperationState* state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context, base::Bind(&DidGetCertDBOnIOThread, token_id, callback, state));

  if (cert_db)
    DidGetCertDBOnIOThread(token_id, callback, state, cert_db);
}

// Asynchronously fetches the NSSCertDatabase for |browser_context| and, if
// |token_id| is not empty, the slot for |token_id|. Stores the slot in |state|
// and passes the database to |callback|. Will run |callback| on the IO thread.
void GetCertDatabase(const std::string& token_id,
                     const GetCertDBCallback& callback,
                     BrowserContext* browser_context,
                     NSSOperationState* state) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GetCertDatabaseOnIOThread,
                                     token_id,
                                     callback,
                                     browser_context->GetResourceContext(),
                                     state));
}

class GenerateRSAKeyState : public NSSOperationState {
 public:
  GenerateRSAKeyState(unsigned int modulus_length_bits,
                      const subtle::GenerateKeyCallback& callback);
  virtual ~GenerateRSAKeyState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from, std::string() /* no public key */, error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                const std::string& public_key_spki_der,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, public_key_spki_der, error_message));
  }

  const unsigned int modulus_length_bits_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  subtle::GenerateKeyCallback callback_;
};

class SignState : public NSSOperationState {
 public:
  SignState(const std::string& public_key,
            HashAlgorithm hash_algorithm,
            const std::string& data,
            const subtle::SignCallback& callback);
  virtual ~SignState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from, std::string() /* no signature */, error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                const std::string& signature,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, signature, error_message));
  }

  const std::string public_key_;
  HashAlgorithm hash_algorithm_;
  const std::string data_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  subtle::SignCallback callback_;
};

class GetCertificatesState : public NSSOperationState {
 public:
  explicit GetCertificatesState(const GetCertificatesCallback& callback);
  virtual ~GetCertificatesState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from,
             scoped_ptr<net::CertificateList>() /* no certificates */,
             error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                scoped_ptr<net::CertificateList> certs,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, base::Passed(&certs), error_message));
  }

  scoped_ptr<net::CertificateList> certs_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetCertificatesCallback callback_;
};

class ImportCertificateState : public NSSOperationState {
 public:
  ImportCertificateState(scoped_refptr<net::X509Certificate> certificate,
                         const ImportCertificateCallback& callback);
  virtual ~ImportCertificateState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from, error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                const std::string& error_message) {
    origin_task_runner_->PostTask(from, base::Bind(callback_, error_message));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  ImportCertificateCallback callback_;
};

class RemoveCertificateState : public NSSOperationState {
 public:
  RemoveCertificateState(scoped_refptr<net::X509Certificate> certificate,
                         const RemoveCertificateCallback& callback);
  virtual ~RemoveCertificateState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from, error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                const std::string& error_message) {
    origin_task_runner_->PostTask(from, base::Bind(callback_, error_message));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  RemoveCertificateCallback callback_;
};

class GetTokensState : public NSSOperationState {
 public:
  explicit GetTokensState(const GetTokensCallback& callback);
  virtual ~GetTokensState() {}

  virtual void OnError(const tracked_objects::Location& from,
                       const std::string& error_message) OVERRIDE {
    CallBack(from,
             scoped_ptr<std::vector<std::string> >() /* no token ids */,
             error_message);
  }

  void CallBack(const tracked_objects::Location& from,
                scoped_ptr<std::vector<std::string> > token_ids,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, base::Passed(&token_ids), error_message));
  }

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetTokensCallback callback_;
};

NSSOperationState::NSSOperationState()
    : origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

GenerateRSAKeyState::GenerateRSAKeyState(
    unsigned int modulus_length_bits,
    const subtle::GenerateKeyCallback& callback)
    : modulus_length_bits_(modulus_length_bits), callback_(callback) {
}

SignState::SignState(const std::string& public_key,
                     HashAlgorithm hash_algorithm,
                     const std::string& data,
                     const subtle::SignCallback& callback)
    : public_key_(public_key),
      hash_algorithm_(hash_algorithm),
      data_(data),
      callback_(callback) {
}

GetCertificatesState::GetCertificatesState(
    const GetCertificatesCallback& callback)
    : callback_(callback) {
}

ImportCertificateState::ImportCertificateState(
    scoped_refptr<net::X509Certificate> certificate,
    const ImportCertificateCallback& callback)
    : certificate_(certificate), callback_(callback) {
}

RemoveCertificateState::RemoveCertificateState(
    scoped_refptr<net::X509Certificate> certificate,
    const RemoveCertificateCallback& callback)
    : certificate_(certificate), callback_(callback) {
}

GetTokensState::GetTokensState(const GetTokensCallback& callback)
    : callback_(callback) {
}

// Does the actual key generation on a worker thread. Used by
// GenerateRSAKeyWithDB().
void GenerateRSAKeyOnWorkerThread(scoped_ptr<GenerateRSAKeyState> state) {
  scoped_ptr<crypto::RSAPrivateKey> rsa_key(
      crypto::RSAPrivateKey::CreateSensitive(state->slot_.get(),
                                             state->modulus_length_bits_));
  if (!rsa_key) {
    LOG(ERROR) << "Couldn't create key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  std::vector<uint8> public_key_spki_der;
  if (!rsa_key->ExportPublicKey(&public_key_spki_der)) {
    // TODO(pneubeck): Remove rsa_key from storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }
  state->CallBack(
      FROM_HERE,
      std::string(public_key_spki_der.begin(), public_key_spki_der.end()),
      std::string() /* no error */);
}

// Continues generating a RSA key with the obtained NSSCertDatabase. Used by
// GenerateRSAKey().
void GenerateRSAKeyWithDB(scoped_ptr<GenerateRSAKeyState> state,
                          net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&GenerateRSAKeyOnWorkerThread, base::Passed(&state)),
      true /*task is slow*/);
}

// Does the actual signing on a worker thread. Used by RSASignWithDB().
void RSASignOnWorkerThread(scoped_ptr<SignState> state) {
  const uint8* public_key_uint8 =
      reinterpret_cast<const uint8*>(state->public_key_.data());
  std::vector<uint8> public_key_vector(
      public_key_uint8, public_key_uint8 + state->public_key_.size());

  // TODO(pneubeck): This searches all slots. Change to look only at |slot_|.
  scoped_ptr<crypto::RSAPrivateKey> rsa_key(
      crypto::RSAPrivateKey::FindFromPublicKeyInfo(public_key_vector));
  if (!rsa_key || rsa_key->key()->pkcs11Slot != state->slot_) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  SECOidTag sign_alg_tag = SEC_OID_UNKNOWN;
  switch (state->hash_algorithm_) {
    case HASH_ALGORITHM_SHA1:
      sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
      break;
    case HASH_ALGORITHM_SHA256:
      sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      break;
    case HASH_ALGORITHM_SHA384:
      sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
      break;
    case HASH_ALGORITHM_SHA512:
      sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
      break;
  }

  SECItem sign_result = {siBuffer, NULL, 0};
  if (SEC_SignData(&sign_result,
                   reinterpret_cast<const unsigned char*>(state->data_.data()),
                   state->data_.size(),
                   rsa_key->key(),
                   sign_alg_tag) != SECSuccess) {
    LOG(ERROR) << "Couldn't sign.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  std::string signature(reinterpret_cast<const char*>(sign_result.data),
                        sign_result.len);
  state->CallBack(FROM_HERE, signature, std::string() /* no error */);
}

// Continues signing with the obtained NSSCertDatabase. Used by Sign().
void RSASignWithDB(scoped_ptr<SignState> state, net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&RSASignOnWorkerThread, base::Passed(&state)),
      true /*task is slow*/);
}

// Filters the obtained certificates on a worker thread. Used by
// DidGetCertificates().
void FilterCertificatesOnWorkerThread(scoped_ptr<GetCertificatesState> state) {
  scoped_ptr<net::CertificateList> client_certs(new net::CertificateList);
  for (net::CertificateList::const_iterator it = state->certs_->begin();
       it != state->certs_->end();
       ++it) {
    net::X509Certificate::OSCertHandle cert_handle = (*it)->os_cert_handle();
    crypto::ScopedPK11Slot cert_slot(PK11_KeyForCertExists(cert_handle,
                                                           NULL,    // keyPtr
                                                           NULL));  // wincx

    // Keep only user certificates, i.e. certs for which the private key is
    // present and stored in the queried slot.
    if (cert_slot != state->slot_)
      continue;

    client_certs->push_back(*it);
  }

  state->CallBack(FROM_HERE, client_certs.Pass(), std::string() /* no error */);
}

// Passes the obtained certificates to the worker thread for filtering. Used by
// GetCertificatesWithDB().
void DidGetCertificates(scoped_ptr<GetCertificatesState> state,
                        scoped_ptr<net::CertificateList> all_certs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state->certs_ = all_certs.Pass();
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&FilterCertificatesOnWorkerThread, base::Passed(&state)),
      true /*task is slow*/);
}

// Continues getting certificates with the obtained NSSCertDatabase. Used by
// GetCertificates().
void GetCertificatesWithDB(scoped_ptr<GetCertificatesState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Get the pointer to slot before base::Passed releases |state|.
  PK11SlotInfo* slot = state->slot_.get();
  cert_db->ListCertsInSlot(
      base::Bind(&DidGetCertificates, base::Passed(&state)), slot);
}

// Does the actual certificate importing on the IO thread. Used by
// ImportCertificate().
void ImportCertificateWithDB(scoped_ptr<ImportCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(pneubeck): Use |state->slot_| to verify that we're really importing to
  // the correct token.
  // |cert_db| is not required, ignore it.
  net::CertDatabase* db = net::CertDatabase::GetInstance();

  const net::Error cert_status =
      static_cast<net::Error>(db->CheckUserCert(state->certificate_.get()));
  if (cert_status == net::ERR_NO_PRIVATE_KEY_FOR_CERT) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  } else if (cert_status != net::OK) {
    state->OnError(FROM_HERE, net::ErrorToString(cert_status));
    return;
  }

  // Check that the private key is in the correct slot.
  PK11SlotInfo* slot =
      PK11_KeyForCertExists(state->certificate_->os_cert_handle(), NULL, NULL);
  if (slot != state->slot_) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  const net::Error import_status =
      static_cast<net::Error>(db->AddUserCert(state->certificate_.get()));
  if (import_status != net::OK) {
    LOG(ERROR) << "Could not import certificate.";
    state->OnError(FROM_HERE, net::ErrorToString(import_status));
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Called on IO thread after the certificate removal is finished.
void DidRemoveCertificate(scoped_ptr<RemoveCertificateState> state,
                          bool certificate_found,
                          bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // CertificateNotFound error has precedence over an internal error.
  if (!certificate_found) {
    state->OnError(FROM_HERE, kErrorCertificateNotFound);
    return;
  }
  if (!success) {
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Does the actual certificate removal on the IO thread. Used by
// RemoveCertificate().
void RemoveCertificateWithDB(scoped_ptr<RemoveCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Get the pointer before base::Passed clears |state|.
  scoped_refptr<net::X509Certificate> certificate = state->certificate_;
  bool certificate_found = certificate->os_cert_handle()->isperm;
  cert_db->DeleteCertAndKeyAsync(
      certificate,
      base::Bind(
          &DidRemoveCertificate, base::Passed(&state), certificate_found));
}

// Does the actual work to determine which tokens are available.
void GetTokensWithDB(scoped_ptr<GetTokensState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<std::vector<std::string> > token_ids(new std::vector<std::string>);

  // The user's token is always available.
  token_ids->push_back(kTokenIdUser);
  if (cert_db->GetSystemSlot())
    token_ids->push_back(kTokenIdSystem);

  state->CallBack(FROM_HERE, token_ids.Pass(), std::string() /* no error */);
}

}  // namespace

namespace subtle {

void GenerateRSAKey(const std::string& token_id,
                    unsigned int modulus_length_bits,
                    const GenerateKeyCallback& callback,
                    BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<GenerateRSAKeyState> state(
      new GenerateRSAKeyState(modulus_length_bits, callback));

  if (modulus_length_bits > kMaxRSAModulusLengthBits) {
    state->OnError(FROM_HERE, kErrorAlgorithmNotSupported);
    return;
  }

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GenerateRSAKeyWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void Sign(const std::string& token_id,
          const std::string& public_key,
          HashAlgorithm hash_algorithm,
          const std::string& data,
          const SignCallback& callback,
          BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<SignState> state(
      new SignState(public_key, hash_algorithm, data, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id,
                  base::Bind(&RSASignWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

}  // namespace subtle

void GetCertificates(const std::string& token_id,
                     const GetCertificatesCallback& callback,
                     BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<GetCertificatesState> state(new GetCertificatesState(callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GetCertificatesWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void ImportCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const ImportCertificateCallback& callback,
                       BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ImportCertificateState> state(
      new ImportCertificateState(certificate, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id,
                  base::Bind(&ImportCertificateWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void RemoveCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const RemoveCertificateCallback& callback,
                       BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<RemoveCertificateState> state(
      new RemoveCertificateState(certificate, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages.
  GetCertDatabase(token_id,
                  base::Bind(&RemoveCertificateWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void GetTokens(const GetTokensCallback& callback,
               content::BrowserContext* browser_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<GetTokensState> state(new GetTokensState(callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(std::string() /* don't get any specific slot */,
                  base::Bind(&GetTokensWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

}  // namespace platform_keys

}  // namespace chromeos
