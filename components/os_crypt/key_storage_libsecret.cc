// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_libsecret.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/libsecret_util_linux.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const char kApplicationName[] = "chrome";
#else
const char kApplicationName[] = "chromium";
#endif

// Deprecated in M55 (crbug.com/639298)
const SecretSchema kKeystoreSchemaV1 = {
    "chrome_libsecret_os_crypt_password",
    SECRET_SCHEMA_NONE,
    {
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

const SecretSchema kKeystoreSchemaV2 = {
    "chrome_libsecret_os_crypt_password_v2",
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {
        {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

}  // namespace

std::string KeyStorageLibsecret::AddRandomPasswordInLibsecret() {
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);
  GError* error = nullptr;
  bool success = LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchemaV2, nullptr, KeyStorageLinux::kKey, password.c_str(),
      nullptr, &error, "application", kApplicationName, nullptr);
  if (error || !success) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    return std::string();
  }

  VLOG(1) << "OSCrypt generated a new password.";
  return password;
}

std::string KeyStorageLibsecret::GetKey() {
  GError* error = nullptr;
  LibsecretAttributesBuilder attrs;
  attrs.Append("application", kApplicationName);
  SecretValue* password_libsecret = LibsecretLoader::secret_service_lookup_sync(
      nullptr, &kKeystoreSchemaV2, attrs.Get(), nullptr, &error);
  if (error) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    g_error_free(error);
    return std::string();
  }
  if (!password_libsecret) {
    std::string password = Migrate();
    if (!password.empty())
      return password;
    return AddRandomPasswordInLibsecret();
  }
  std::string password(
      LibsecretLoader::secret_value_get_text(password_libsecret));
  LibsecretLoader::secret_value_unref(password_libsecret);
  return password;
}

bool KeyStorageLibsecret::Init() {
  return LibsecretLoader::EnsureLibsecretLoaded();
}

std::string KeyStorageLibsecret::Migrate() {
  GError* error = nullptr;
  LibsecretAttributesBuilder attrs;

  // Detect old entry.
  SecretValue* password_libsecret = LibsecretLoader::secret_service_lookup_sync(
      nullptr, &kKeystoreSchemaV1, attrs.Get(), nullptr, &error);
  if (error || !password_libsecret)
    return std::string();

  VLOG(1) << "OSCrypt detected a deprecated password in Libsecret.";
  std::string password(
      LibsecretLoader::secret_value_get_text(password_libsecret));

  // Create new entry.
  bool success = LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchemaV2, nullptr, KeyStorageLinux::kKey, password.c_str(),
      nullptr, &error, "application", kApplicationName, nullptr);
  if (error || !success)
    return std::string();

  // Delete old entry.
  // Even if deletion failed, we have to use the password that we created.
  success = LibsecretLoader::secret_password_clear_sync(
      &kKeystoreSchemaV1, nullptr, &error, nullptr);

  VLOG(1) << "OSCrypt migrated from deprecated password.";

  return password;
}
