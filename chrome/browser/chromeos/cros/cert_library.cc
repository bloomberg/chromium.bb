// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cert_library.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"  // g_browser_process
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"
#include "grit/generated_resources.h"
#include "net/base/cert_database.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "unicode/coll.h"  // icu::Collator

using content::BrowserThread;

namespace chromeos {

namespace {

// Root CA certificates that are built into Chrome use this token name.
const char kRootCertificateTokenName[] = "Builtin Object Token";

// Delay between certificate requests while waiting for TPM/PKCS#11 init.
const int kRequestDelayMs = 500;

const size_t kKeySize = 16;

// Decrypts (AES) hex encoded encrypted token given |key| and |salt|.
std::string DecryptTokenWithKey(
    crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& encrypted_token_hex) {
  std::vector<uint8> encrypted_token_bytes;
  if (!base::HexStringToBytes(encrypted_token_hex, &encrypted_token_bytes))
    return std::string();

  std::string encrypted_token(
      reinterpret_cast<char*>(encrypted_token_bytes.data()),
      encrypted_token_bytes.size());
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string()))
    return std::string();

  std::string nonce = salt.substr(0, kKeySize);
  std::string token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Decrypt(encrypted_token, &token))
    return std::string();
  return token;
}

string16 GetDisplayString(net::X509Certificate* cert, bool hardware_backed) {
  std::string org;
  if (!cert->subject().organization_names.empty())
    org = cert->subject().organization_names[0];
  if (org.empty())
    org = cert->subject().GetDisplayName();
  string16 issued_by = UTF8ToUTF16(
      x509_certificate_model::GetIssuerCommonName(cert->os_cert_handle(),
                                                  org));  // alternative text
  string16 issued_to = UTF8ToUTF16(
      x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle()));

  if (hardware_backed) {
    return l10n_util::GetStringFUTF16(
        IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT_LONG,
        issued_by,
        issued_to,
        l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CERT_MANAGER_KEY_FORMAT_LONG,
        issued_by,
        issued_to);
  }
}

class RealTPMTokenInfoDelegate : public crypto::TPMTokenInfoDelegate {
 public:
  RealTPMTokenInfoDelegate();
  virtual ~RealTPMTokenInfoDelegate();

  // TPMTokenInfoDeleagte overrides:
  virtual bool IsTokenAvailable() const OVERRIDE;
  virtual void RequestIsTokenReady(
      base::Callback<void(bool result)> callback) const OVERRIDE;
  virtual void GetTokenInfo(std::string* token_name,
                            std::string* user_pin) const OVERRIDE;

 private:
  // This method is used to implement RequestIsTokenReady.
  void OnPkcs11IsTpmTokenReady(base::Callback<void(bool result)> callback,
                               DBusMethodCallStatus call_status,
                               bool is_tpm_token_ready) const;

  // This method is used to implement RequestIsTokenReady.
  void OnPkcs11GetTpmTokenInfo(base::Callback<void(bool result)> callback,
                               DBusMethodCallStatus call_status,
                               const std::string& token_name,
                               const std::string& user_pin) const;

  // These are mutable since we need to cache them in IsTokenReady().
  mutable bool token_ready_;
  mutable std::string token_name_;
  mutable std::string user_pin_;
  mutable base::WeakPtrFactory<RealTPMTokenInfoDelegate> weak_ptr_factory_;
};

RealTPMTokenInfoDelegate::RealTPMTokenInfoDelegate() : token_ready_(false),
                                                       weak_ptr_factory_(this) {
}

RealTPMTokenInfoDelegate::~RealTPMTokenInfoDelegate() {}

bool RealTPMTokenInfoDelegate::IsTokenAvailable() const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool result = false;
  // TODO(hashimoto): Use non-blocking method instead. crbug.com/126674
  DBusThreadManager::Get()->GetCryptohomeClient()->CallTpmIsEnabledAndBlock(
      &result);
  return result;
}

void RealTPMTokenInfoDelegate::RequestIsTokenReady(
    base::Callback<void(bool result)> callback) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (token_ready_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, true));
    return;
  }
  DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11IsTpmTokenReady(
      base::Bind(&RealTPMTokenInfoDelegate::OnPkcs11IsTpmTokenReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void RealTPMTokenInfoDelegate::GetTokenInfo(std::string* token_name,
                                            std::string* user_pin) const {
  // May be called from a non UI thread, but must only be called after
  // IsTokenReady() returns true.
  CHECK(token_ready_);
  if (token_name)
    *token_name = token_name_;
  if (user_pin)
    *user_pin = user_pin_;
}

void RealTPMTokenInfoDelegate::OnPkcs11IsTpmTokenReady(
    base::Callback<void(bool result)> callback,
    DBusMethodCallStatus call_status,
    bool is_tpm_token_ready) const {
  if (call_status != DBUS_METHOD_CALL_SUCCESS || !is_tpm_token_ready) {
    callback.Run(false);
    return;
  }

  // Retrieve token_name_ and user_pin_ here since they will never change
  // and CryptohomeClient calls are not thread safe.
  DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11GetTpmTokenInfo(
      base::Bind(&RealTPMTokenInfoDelegate::OnPkcs11GetTpmTokenInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void RealTPMTokenInfoDelegate::OnPkcs11GetTpmTokenInfo(
    base::Callback<void(bool result)> callback,
    DBusMethodCallStatus call_status,
    const std::string& token_name,
    const std::string& user_pin) const {
  if (call_status == DBUS_METHOD_CALL_SUCCESS) {
    token_name_ = token_name;
    user_pin_ = user_pin;
    token_ready_ = true;
  }
  callback.Run(token_ready_);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////

// base::Unretained(this) in the class is safe. By the time this object is
// deleted as part of CrosLibrary, the DB thread and the UI message loop
// are already terminated.
class CertLibraryImpl
    : public CertLibrary,
      public net::CertDatabase::Observer {
 public:
  typedef ObserverListThreadSafe<CertLibrary::Observer> CertLibraryObserverList;

  CertLibraryImpl() :
      observer_list_(new CertLibraryObserverList),
      user_logged_in_(false),
      certificates_requested_(false),
      certificates_loaded_(false),
      key_store_loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(certs_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(user_certs_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(server_certs_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(server_ca_certs_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    net::CertDatabase::AddObserver(this);
  }

  ~CertLibraryImpl() {
    DCHECK(request_task_.is_null());
    net::CertDatabase::RemoveObserver(this);
  }

  // CertLibrary implementation.
  virtual void AddObserver(CertLibrary::Observer* observer) OVERRIDE {
    observer_list_->AddObserver(observer);
  }

  virtual void RemoveObserver(CertLibrary::Observer* observer) OVERRIDE {
    observer_list_->RemoveObserver(observer);
  }

  virtual void LoadKeyStore() OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (key_store_loaded_)
      return;

    // Ensure we've opened the real user's key/certificate database.
    crypto::OpenPersistentNSSDB();

    // Only load the Opencryptoki library into NSS if we have this switch.
    // TODO(gspencer): Remove this switch once cryptohomed work is finished:
    // http://crosbug.com/12295 and 12304
    // Note: ChromeOS login with or without loginmanager will crash when
    // the CertLibrary is not there (http://crosbug.com/121456). Before removing
    // make sure that that case still works.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLoadOpencryptoki) ||
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kStubCros)) {
      crypto::EnableTPMTokenForNSS(new RealTPMTokenInfoDelegate());
      // Note: this calls crypto::EnsureTPMTokenReady()
      RequestCertificates();
    }
    key_store_loaded_ = true;
  }

  virtual bool CertificatesLoading() const OVERRIDE {
    return certificates_requested_ && !certificates_loaded_;
  }

  virtual bool CertificatesLoaded() const OVERRIDE {
    return certificates_loaded_;
  }

  virtual bool IsHardwareBacked() const OVERRIDE {
    return !tpm_token_name_.empty();
  }

  virtual const CertList& GetCertificates() const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    return certs_;
  }

  virtual const CertList& GetUserCertificates() const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    return user_certs_;
  }

  virtual const CertList& GetServerCertificates() const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    return server_certs_;
  }

  virtual const CertList& GetCACertificates() const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    return server_ca_certs_;
  }

  virtual std::string EncryptToken(const std::string& token) OVERRIDE {
    if (!LoadSupplementalUserKey())
      return std::string();
    crypto::Encryptor encryptor;
    if (!encryptor.Init(supplemental_user_key_.get(), crypto::Encryptor::CTR,
                        std::string()))
      return std::string();
    std::string salt =
        CrosLibrary::Get()->GetCryptohomeLibrary()->GetSystemSalt();
    std::string nonce = salt.substr(0, kKeySize);
    std::string encoded_token;
    CHECK(encryptor.SetCounter(nonce));
    if (!encryptor.Encrypt(token, &encoded_token))
      return std::string();

    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(encoded_token.data()),
        encoded_token.size()));
  }

  virtual std::string DecryptToken(
      const std::string& encrypted_token_hex) OVERRIDE {
    if (!LoadSupplementalUserKey())
      return std::string();
    return DecryptTokenWithKey(supplemental_user_key_.get(),
        CrosLibrary::Get()->GetCryptohomeLibrary()->GetSystemSalt(),
        encrypted_token_hex);
  }

  // net::CertDatabase::Observer implementation. Observer added on UI thread.
  virtual void OnCertTrustChanged(const net::X509Certificate* cert) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
  }

  virtual void OnUserCertAdded(const net::X509Certificate* cert) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    // Only load certificates if we have completed an initial request.
    if (certificates_loaded_) {
      BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&CertLibraryImpl::LoadCertificates,
                     base::Unretained(this)));
    }
  }

  virtual void OnUserCertRemoved(const net::X509Certificate* cert) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    // Only load certificates if we have completed an initial request.
    if (certificates_loaded_) {
      BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&CertLibraryImpl::LoadCertificates,
                     base::Unretained(this)));
    }
  }

  virtual const std::string& GetTpmTokenName() const OVERRIDE {
    return tpm_token_name_;
  }

 private:
  void LoadCertificates() {
    VLOG(1) << " Loading Certificates.";
    // Certificate fetch occurs on the DB thread.
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB))
        << __FUNCTION__ << " should be called on DB thread.";
    net::CertDatabase cert_db;
    net::CertificateList* cert_list = new net::CertificateList();
    cert_db.ListCerts(cert_list);
    // Pass the list to the UI thread to safely update the local lists.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CertLibraryImpl::UpdateCertificates,
                   base::Unretained(this), cert_list));
  }

  // Comparison functor for locale-sensitive sorting of certificates by name.
  class CertNameComparator {
   public:
    explicit CertNameComparator(icu::Collator* collator)
        : collator_(collator) { }

    bool operator()(const scoped_refptr<net::X509Certificate>& lhs,
                    const scoped_refptr<net::X509Certificate>& rhs) const {
      string16 lhs_name = GetDisplayString(lhs.get(), false);
      string16 rhs_name = GetDisplayString(rhs.get(), false);
      if (collator_ == NULL)
        return lhs_name < rhs_name;
      return l10n_util::CompareString16WithCollator(
          collator_, lhs_name, rhs_name) == UCOL_LESS;
    }
   private:
    icu::Collator* collator_;
  };

  void RequestCertificatesTask() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    // Reset the task to the initial state so is_null() returns true.
    request_task_ = base::Closure();
    RequestCertificates();
  }

  void NotifyCertificatesLoaded(bool initial_load) {
    observer_list_->Notify(
        &CertLibrary::Observer::OnCertificatesLoaded, initial_load);
  }

  // |cert_list| is allocated in LoadCertificates() and must be deleted here.
  void UpdateCertificates(net::CertificateList* cert_list) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    DCHECK(cert_list);

    // Clear any existing certificates.
    certs_.Clear();
    server_ca_certs_.Clear();
    user_certs_.Clear();
    server_certs_.Clear();

    // Add certificates to the appropriate list.
    for (net::CertificateList::const_iterator iter = cert_list->begin();
         iter != cert_list->end(); ++iter) {
      certs_.Append(iter->get());
      net::X509Certificate::OSCertHandle cert_handle =
          iter->get()->os_cert_handle();
      net::CertType type = x509_certificate_model::GetType(cert_handle);
      switch (type) {
        case net::USER_CERT:
          user_certs_.Append(iter->get());
          break;
        case net::SERVER_CERT:
          server_certs_.Append(iter->get());
          break;
        case net::CA_CERT: {
          // Exclude root CA certificates that are built into Chrome.
          std::string token_name =
              x509_certificate_model::GetTokenName(cert_handle);
          if (token_name != kRootCertificateTokenName)
            server_ca_certs_.Append(iter->get());
          break;
        }
        default:
          break;
      }
    }

    // Perform locale-sensitive sorting by certificate name.
    scoped_ptr<icu::Collator> collator;
    UErrorCode error = U_ZERO_ERROR;
    collator.reset(
        icu::Collator::createInstance(
            icu::Locale(g_browser_process->GetApplicationLocale().c_str()),
            error));
    if (U_FAILURE(error))
      collator.reset(NULL);
    CertNameComparator cert_name_comparator(collator.get());
    std::sort(user_certs_.list().begin(), user_certs_.list().end(),
              cert_name_comparator);
    std::sort(server_certs_.list().begin(), server_certs_.list().end(),
              cert_name_comparator);
    std::sort(server_ca_certs_.list().begin(), server_ca_certs_.list().end(),
              cert_name_comparator);

    // cert_list is allocated in LoadCertificates(), then released here.
    delete cert_list;

    // Set loaded state and notify observers.
    if (!certificates_loaded_) {
      certificates_loaded_ = true;
      NotifyCertificatesLoaded(true);
    } else {
      NotifyCertificatesLoaded(false);
    }
  }

  bool LoadSupplementalUserKey() {
    if (!user_logged_in_) {
      // If we are not logged in, we cannot load any certificates.
      // Set 'loaded' to true for the UI, since we are not waiting on loading.
      LOG(WARNING) << "Requesting supplemental use key before login.";
      return false;
    }
    if (!supplemental_user_key_.get()) {
      supplemental_user_key_.reset(crypto::GetSupplementalUserKey());
    }
    return supplemental_user_key_.get() != NULL;
  }

  // Call this to start the certificate list initialization process.
  // Must be called from the UI thread.
  void RequestCertificates() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";

    certificates_requested_ = true;

    if (!UserManager::Get()->IsUserLoggedIn()) {
      // If we are not logged in, we cannot load any certificates.
      // Set 'loaded' to true for the UI, since we are not waiting on loading.
      LOG(WARNING) << "Requesting certificates before login.";
      certificates_loaded_ = true;
      supplemental_user_key_.reset(NULL);
      return;
    }

    if (!user_logged_in_) {
      user_logged_in_ = true;
      certificates_loaded_ = false;
      supplemental_user_key_.reset(NULL);
    }

    VLOG(1) << "Requesting Certificates.";
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsEnabled(
        base::Bind(&CertLibraryImpl::RequestCertificatesInternal,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // This method is used to implement RequestCertificates.
  void RequestCertificatesInternal(DBusMethodCallStatus call_status,
                                   bool tpm_is_enabled) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    if (call_status != DBUS_METHOD_CALL_SUCCESS || !tpm_is_enabled) {
      // TPM is not enabled, so proceed with empty tpm token name.
      VLOG(1) << "TPM not available.";
      BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&CertLibraryImpl::LoadCertificates,
                     base::Unretained(this)));
    } else if (crypto::IsTPMTokenReady()) {
      // Need TPM token name to filter user certificates.
      const bool tpm_token_ready = true;
      GetTPMTokenName(tpm_token_ready);
    } else {
      crypto::InitializeTPMToken(
          base::Bind(&CertLibraryImpl::GetTPMTokenName,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // This method is used to implement RequestCertificates.
  void GetTPMTokenName(bool tpm_token_ready) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << __FUNCTION__ << " should be called on UI thread.";
    if (tpm_token_ready) {
      std::string unused_pin;
      crypto::GetTPMTokenInfo(&tpm_token_name_, &unused_pin);
    } else {
      VLOG(1) << "TPM token not ready.";
      if (request_task_.is_null()) {
        // Cryptohome does not notify us when the token is ready, so call
        // this again after a delay.
        request_task_ = base::Bind(&CertLibraryImpl::RequestCertificatesTask,
                                   weak_ptr_factory_.GetWeakPtr());
        BrowserThread::PostDelayedTask(
            BrowserThread::UI, FROM_HERE, request_task_,
            base::TimeDelta::FromMilliseconds(kRequestDelayMs));
      }
      return;
    }

    // tpm_token_name_ is set, load the certificates on the DB thread.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&CertLibraryImpl::LoadCertificates, base::Unretained(this)));
  }

  // Observers.
  const scoped_refptr<CertLibraryObserverList> observer_list_;

  // Active request task for re-requests while waiting for TPM init.
  base::Closure request_task_;

  // Cached TPM token name.
  std::string tpm_token_name_;

  // Supplemental user key.
  scoped_ptr<crypto::SymmetricKey> supplemental_user_key_;

  // Local state.
  bool user_logged_in_;
  bool certificates_requested_;
  bool certificates_loaded_;
  // The key store for the current user has been loaded. This flag is needed to
  // ensure that the key store will not be loaded twice in the policy recovery
  // "safe-mode".
  bool key_store_loaded_;

  // Certificates.
  CertList certs_;
  CertList user_certs_;
  CertList server_certs_;
  CertList server_ca_certs_;

  base::WeakPtrFactory<CertLibraryImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLibraryImpl);
};

//////////////////////////////////////////////////////////////////////////////

CertLibrary::~CertLibrary() {
}

// static
CertLibrary* CertLibrary::GetImpl(bool stub) {
  // No libcros dependencies, so always return CertLibraryImpl() (no stub).
  return new CertLibraryImpl();
}

//////////////////////////////////////////////////////////////////////////////

net::X509Certificate* CertLibrary::CertList::GetCertificateAt(int index) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << __FUNCTION__ << " should be called on UI thread.";
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(list_.size()));
  return list_[index].get();
}

string16 CertLibrary::CertList::GetDisplayStringAt(int index) const {
  net::X509Certificate* cert = GetCertificateAt(index);
  bool hardware_backed =
      !cert_library_->GetTpmTokenName().empty() && IsHardwareBackedAt(index);
  return GetDisplayString(cert, hardware_backed);
}

std::string CertLibrary::CertList::GetNicknameAt(int index) const {
  net::X509Certificate* cert = GetCertificateAt(index);
  return x509_certificate_model::GetNickname(cert->os_cert_handle());
}

std::string CertLibrary::CertList::GetPkcs11IdAt(int index) const {
  net::X509Certificate* cert = GetCertificateAt(index);
  return x509_certificate_model::GetPkcs11Id(cert->os_cert_handle());
}

bool CertLibrary::CertList::IsHardwareBackedAt(int index) const {
  net::X509Certificate* cert = GetCertificateAt(index);
  std::string cert_token_name =
      x509_certificate_model::GetTokenName(cert->os_cert_handle());
  return cert_token_name == cert_library_->GetTpmTokenName();
}

int CertLibrary::CertList::FindCertByNickname(
    const std::string& nickname) const {
  for (int index = 0; index < Size(); ++index) {
    net::X509Certificate* cert = GetCertificateAt(index);
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string nick = x509_certificate_model::GetNickname(cert_handle);
    if (nick == nickname)
      return index;
  }
  return -1;  // Not found.
}

int CertLibrary::CertList::FindCertByPkcs11Id(
    const std::string& pkcs11_id) const {
  for (int index = 0; index < Size(); ++index) {
    net::X509Certificate* cert = GetCertificateAt(index);
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string id = x509_certificate_model::GetPkcs11Id(cert_handle);
    if (id == pkcs11_id)
      return index;
  }
  return -1;  // Not found.
}

}  // chromeos
