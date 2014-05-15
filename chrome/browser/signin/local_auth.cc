// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/local_auth.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/os_crypt/os_crypt.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "crypto/symmetric_key.h"

namespace {

// WARNING: Changing these values will make it impossible to do off-line
// authentication until the next successful on-line authentication.  To change
// these safely, change the "encoding" version below and make verification
// handle multiple values.
const char kHash1Encoding = '1';
const unsigned kHash1Bits = 256;
const unsigned kHash1Bytes = kHash1Bits / 8;
const unsigned kHash1IterationCount = 100000;

std::string CreateSecurePasswordHash(const std::string& salt,
                                     const std::string& password,
                                     char encoding) {
  DCHECK_EQ(kHash1Bytes, salt.length());
  DCHECK_EQ(kHash1Encoding, encoding);  // Currently support only one method.

  base::Time start_time = base::Time::Now();

  // Library call to create secure password hash as SymmetricKey (uses PBKDF2).
  scoped_ptr<crypto::SymmetricKey> password_key(
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::AES,
          password, salt,
          kHash1IterationCount, kHash1Bits));
  std::string password_hash;
  const bool success = password_key->GetRawKey(&password_hash);
  DCHECK(success);
  DCHECK_EQ(kHash1Bytes, password_hash.length());

  UMA_HISTOGRAM_TIMES("PasswordHash.CreateTime",
                      base::Time::Now() - start_time);

  return password_hash;
}

std::string EncodePasswordHashRecord(const std::string& record,
                                     char encoding) {
  DCHECK_EQ(kHash1Encoding, encoding);  // Currently support only one method.

  // Encrypt the hash using the OS account-password protection (if available).
  std::string encoded;
  const bool success = OSCrypt::EncryptString(record, &encoded);
  DCHECK(success);

  // Convert binary record to text for preference database.
  std::string encoded64;
  base::Base64Encode(encoded, &encoded64);

  // Stuff the "encoding" value into the first byte.
  encoded64.insert(0, &encoding, sizeof(encoding));

  return encoded64;
}

bool DecodePasswordHashRecord(const std::string& encoded,
                              std::string* decoded,
                              char* encoding) {
  // Extract the "encoding" value from the first byte and validate.
  if (encoded.length() < 1)
    return false;
  *encoding = encoded[0];
  if (*encoding != kHash1Encoding)
    return false;

  // Stored record is base64; convert to binary.
  std::string unbase64;
  if (!base::Base64Decode(encoded.substr(1), &unbase64))
    return false;

  // Decrypt the record using the OS account-password protection (if available).
  return OSCrypt::DecryptString(unbase64, decoded);
}

}  // namespace

namespace chrome {

void RegisterLocalAuthPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kGoogleServicesPasswordHash,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void SetLocalAuthCredentials(size_t info_index,
                             const std::string& password) {
  DCHECK(password.length());

  // Salt should be random data, as long as the hash length, and different with
  // every save.
  std::string salt_str;
  crypto::RandBytes(WriteInto(&salt_str, kHash1Bytes + 1), kHash1Bytes);
  DCHECK_EQ(kHash1Bytes, salt_str.length());

  // Perform secure hash of password for storage.
  std::string password_hash = CreateSecurePasswordHash(
      salt_str, password, kHash1Encoding);
  DCHECK_EQ(kHash1Bytes, password_hash.length());

  // Group all fields into a single record for storage;
  std::string record;
  record.append(salt_str);
  record.append(password_hash);

  // Encode it and store it.
  std::string encoded = EncodePasswordHashRecord(record, kHash1Encoding);
  ProfileInfoCache& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  info.SetLocalAuthCredentialsOfProfileAtIndex(info_index, encoded);
}

void SetLocalAuthCredentials(const Profile* profile,
                             const std::string& password) {
  DCHECK(profile);

  ProfileInfoCache& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t info_index = info.GetIndexOfProfileWithPath(profile->GetPath());
  if (info_index == std::string::npos) {
    NOTREACHED();
    return;
  }
  SetLocalAuthCredentials(info_index, password);
}

bool ValidateLocalAuthCredentials(size_t info_index,
                                  const std::string& password) {
  std::string record;
  char encoding;

  ProfileInfoCache& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  std::string encodedhash =
      info.GetLocalAuthCredentialsOfProfileAtIndex(info_index);
  if (encodedhash.length() == 0 && password.length() == 0)
    return true;
  if (!DecodePasswordHashRecord(encodedhash, &record, &encoding))
    return false;

  std::string password_hash;
  const char* password_saved;
  const char* password_check;
  size_t password_length;

  if (encoding == '1') {
    // Validate correct length; extract salt and password hash.
    if (record.length() != 2 * kHash1Bytes)
      return false;
    std::string salt_str(record.data(), kHash1Bytes);
    password_saved = record.data() + kHash1Bytes;
    password_hash = CreateSecurePasswordHash(salt_str, password, encoding);
    password_check = password_hash.data();
    password_length = kHash1Bytes;
  } else {
    // unknown encoding
    return false;
  }

  return crypto::SecureMemEqual(password_saved, password_check,
                                password_length);
}

bool ValidateLocalAuthCredentials(const Profile* profile,
                                  const std::string& password) {
  DCHECK(profile);

  ProfileInfoCache& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t info_index = info.GetIndexOfProfileWithPath(profile->GetPath());
  if (info_index == std::string::npos) {
    NOTREACHED();  // This should never happen but fail safely if it does.
    return false;
  }
  return ValidateLocalAuthCredentials(info_index, password);
}

}  // namespace chrome
