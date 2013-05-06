// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_library.h"

#include <map>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/memory/weak_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace chromeos {

namespace {

const char kStubSystemSalt[] = "stub_system_salt";
const size_t kNonceSize = 16;

// Does nothing.  Used as a Cryptohome::VoidMethodCallback.
void DoNothing(DBusMethodCallStatus call_status) {}

}  // namespace

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() : weak_ptr_factory_(this) {
  }

  virtual ~CryptohomeLibraryImpl() {
  }

  virtual bool TpmIsEnabled() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->CallTpmIsEnabledAndBlock(
        &result);
    return result;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->CallTpmIsOwnedAndBlock(
        &result);
    return result;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        CallTpmIsBeingOwnedAndBlock(&result);
    return result;
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
        base::Bind(&DoNothing));
  }

  virtual void TpmClearStoredPassword() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        CallTpmClearStoredPasswordAndBlock();
  }

  virtual bool InstallAttributesGet(
      const std::string& name, std::string* value) OVERRIDE {
    std::vector<uint8> buf;
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesGet(name, &buf, &success);
    if (success) {
      // Cryptohome returns 'buf' with a terminating '\0' character.
      DCHECK(!buf.empty());
      DCHECK_EQ(buf.back(), 0);
      value->assign(reinterpret_cast<char*>(buf.data()), buf.size() - 1);
    }
    return success;
  }

  virtual bool InstallAttributesSet(
      const std::string& name, const std::string& value) OVERRIDE {
    std::vector<uint8> buf(value.c_str(), value.c_str() + value.size() + 1);
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesSet(name, buf, &success);
    return success;
  }

  virtual bool InstallAttributesFinalize() OVERRIDE {
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesFinalize(&success);
    return success;
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesIsInvalid(&result);
    return result;
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesIsFirstInstall(&result);
    return result;
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    LoadSystemSalt();  // no-op if it's already loaded.
    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(system_salt_.data()),
        system_salt_.size()));
  }

  virtual std::string EncryptWithSystemSalt(const std::string& token) OVERRIDE {
    // Don't care about token encryption while debugging.
    if (!base::chromeos::IsRunningOnChromeOS())
      return token;

    if (!LoadSystemSaltKey()) {
      LOG(WARNING) << "System salt key is not available for encrypt.";
      return std::string();
    }
    return EncryptTokenWithKey(system_salt_key_.get(),
                               GetSystemSalt(),
                               token);
  }

  virtual std::string DecryptWithSystemSalt(
      const std::string& encrypted_token_hex) OVERRIDE {
    // Don't care about token encryption while debugging.
    if (!base::chromeos::IsRunningOnChromeOS())
      return encrypted_token_hex;

    if (!LoadSystemSaltKey()) {
      LOG(WARNING) << "System salt key is not available for decrypt.";
      return std::string();
    }
    return DecryptTokenWithKey(system_salt_key_.get(),
                               GetSystemSalt(),
                               encrypted_token_hex);
  }

 private:
  void LoadSystemSalt() {
    if (!system_salt_.empty())
      return;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        GetSystemSalt(&system_salt_);
    CHECK(!system_salt_.empty());
    CHECK_EQ(system_salt_.size() % 2, 0U);
  }

  // TODO: should this use the system salt for both the password and the salt
  // value, or should this use a separate salt value?
  bool LoadSystemSaltKey() {
    if (!system_salt_key_.get())
      system_salt_key_.reset(PassphraseToKey(GetSystemSalt(), GetSystemSalt()));
    return system_salt_key_.get();
  }

  crypto::SymmetricKey* PassphraseToKey(const std::string& passphrase,
                                        const std::string& salt) {
    return crypto::SymmetricKey::DeriveKeyFromPassword(
        crypto::SymmetricKey::AES, passphrase, salt, 1000, 256);
  }


  // Encrypts (AES) the token given |key| and |salt|.
  std::string EncryptTokenWithKey(crypto::SymmetricKey* key,
                                  const std::string& salt,
                                  const std::string& token) {
    crypto::Encryptor encryptor;
    if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
      LOG(WARNING) << "Failed to initialize Encryptor.";
      return std::string();
    }
    std::string nonce = salt.substr(0, kNonceSize);
    std::string encoded_token;
    CHECK(encryptor.SetCounter(nonce));
    if (!encryptor.Encrypt(token, &encoded_token)) {
      LOG(WARNING) << "Failed to encrypt token.";
      return std::string();
    }

    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(encoded_token.data()),
        encoded_token.size()));
  }

  // Decrypts (AES) hex encoded encrypted token given |key| and |salt|.
  std::string DecryptTokenWithKey(crypto::SymmetricKey* key,
                                  const std::string& salt,
                                  const std::string& encrypted_token_hex) {
    std::vector<uint8> encrypted_token_bytes;
    if (!base::HexStringToBytes(encrypted_token_hex, &encrypted_token_bytes)) {
      LOG(WARNING) << "Corrupt encrypted token found.";
      return std::string();
    }

    std::string encrypted_token(
        reinterpret_cast<char*>(encrypted_token_bytes.data()),
        encrypted_token_bytes.size());
    crypto::Encryptor encryptor;
    if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
      LOG(WARNING) << "Failed to initialize Encryptor.";
      return std::string();
    }

    std::string nonce = salt.substr(0, kNonceSize);
    std::string token;
    CHECK(encryptor.SetCounter(nonce));
    if (!encryptor.Decrypt(encrypted_token, &token)) {
      LOG(WARNING) << "Failed to decrypt token.";
      return std::string();
    }
    return token;
  }

  base::WeakPtrFactory<CryptohomeLibraryImpl> weak_ptr_factory_;
  std::vector<uint8> system_salt_;
  // A key based on the system salt.  Useful for encrypting device-level
  // data for which we have no additional credentials.
  scoped_ptr<crypto::SymmetricKey> system_salt_key_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl()
    : locked_(false) {}
  virtual ~CryptohomeLibraryStubImpl() {}

  virtual bool TpmIsEnabled() OVERRIDE {
    return true;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    return true;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    return true;
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {}

  virtual void TpmClearStoredPassword() OVERRIDE {}

  virtual bool InstallAttributesGet(
      const std::string& name, std::string* value) OVERRIDE {
    if (install_attrs_.find(name) != install_attrs_.end()) {
      *value = install_attrs_[name];
      return true;
    }
    return false;
  }

  virtual bool InstallAttributesSet(
      const std::string& name, const std::string& value) OVERRIDE {
    install_attrs_[name] = value;
    return true;
  }

  virtual bool InstallAttributesFinalize() OVERRIDE {
    locked_ = true;
    return true;
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    return false;
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    return !locked_;
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    return kStubSystemSalt;
  }

  virtual std::string EncryptWithSystemSalt(const std::string& token) {
    return token;
  }

  virtual std::string DecryptWithSystemSalt(
      const std::string& encrypted_token_hex) {
    return encrypted_token_hex;
  }

 private:
  std::map<std::string, std::string> install_attrs_;
  bool locked_;
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryStubImpl);
};

CryptohomeLibrary::CryptohomeLibrary() {}
CryptohomeLibrary::~CryptohomeLibrary() {}

static CryptohomeLibrary* g_cryptohome_library = NULL;
static CryptohomeLibrary* g_test_cryptohome_library = NULL;

// static
void CryptohomeLibrary::Initialize() {
  CHECK(!g_cryptohome_library);
  if (base::chromeos::IsRunningOnChromeOS())
    g_cryptohome_library = new CryptohomeLibraryImpl();
  else
    g_cryptohome_library = new CryptohomeLibraryStubImpl();
}

// static
bool CryptohomeLibrary::IsInitialized() {
  return g_cryptohome_library;
}

// static
void CryptohomeLibrary::Shutdown() {
  CHECK(g_cryptohome_library);
  delete g_cryptohome_library;
  g_cryptohome_library = NULL;
}

// static
CryptohomeLibrary* CryptohomeLibrary::Get() {
  CHECK(g_cryptohome_library || g_test_cryptohome_library)
      << "CryptohomeLibrary::Get() called before Initialize()";
  if (g_test_cryptohome_library)
    return g_test_cryptohome_library;
  return g_cryptohome_library;
}

// static
void CryptohomeLibrary::SetForTest(CryptohomeLibrary* impl) {
  CHECK(!g_test_cryptohome_library || !impl);
  g_test_cryptohome_library = impl;
}

// static
CryptohomeLibrary* CryptohomeLibrary::GetTestImpl() {
  return new CryptohomeLibraryStubImpl();
}

} // namespace chromeos
