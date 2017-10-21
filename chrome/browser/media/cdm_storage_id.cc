// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_storage_id.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "media/media_features.h"

// TODO(jrummell): Only compile this file if ENABLE_CDM_STORAGE_ID specified.
// TODO(jrummell): Move this to the chrome namespace.

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#include "base/logging.h"
#include "chrome/browser/media/cdm_storage_id_key.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "rlz/features/features.h"

#if defined(OS_CHROMEOS)
#include "base/bind.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "media/base/scoped_callback_runner.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#else
#error "RLZ must be enabled on Windows/Mac"
#endif  // BUILDFLAG(ENABLE_RLZ)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

namespace cdm_storage_id {

namespace {

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)

// Calculates the Storage Id based on:
//   |storage_id_key| - a browser identifier
//   |profile_salt|   - setting in the user's profile
//   |origin|         - the origin used
//   |machine_id|     - a device identifier
// If all the parameters appear valid, this function returns the SHA256
// checksum of the above values. If any of the values are invalid, the empty
// vector is returned.
std::vector<uint8_t> ComputeStorageId(const std::string& storage_id_key,
                                      const std::vector<uint8_t>& profile_salt,
                                      const url::Origin& origin,
                                      const std::string& machine_id) {
  if (storage_id_key.length() < chrome::kMinimumCdmStorageIdKeyLength) {
    DLOG(ERROR) << "Storage key not set correctly, length: "
                << storage_id_key.length();
    return {};
  }

  if (profile_salt.size() != MediaStorageIdSalt::kSaltLength) {
    DLOG(ERROR) << "Unexpected salt bytes length: " << profile_salt.size();
    return {};
  }

  if (origin.unique()) {
    DLOG(ERROR) << "Unexpected origin: " << origin;
    return {};
  }

  if (machine_id.empty()) {
    DLOG(ERROR) << "Empty machine id";
    return {};
  }

  // Build the identifier as follows:
  // SHA256(machine_id + origin + storage_id_key + profile_salt)
  std::string origin_str = origin.Serialize();
  std::unique_ptr<crypto::SecureHash> sha256 =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  sha256->Update(machine_id.data(), machine_id.length());
  sha256->Update(origin_str.data(), origin_str.length());
  sha256->Update(storage_id_key.data(), storage_id_key.length());
  sha256->Update(profile_salt.data(), profile_salt.size());

  std::vector<uint8_t> result(crypto::kSHA256Length);
  sha256->Finish(result.data(), result.size());
  return result;
}

#if defined(OS_CHROMEOS)
void ComputeAndReturnStorageId(const std::vector<uint8_t>& profile_salt,
                               const url::Origin& origin,
                               CdmStorageIdCallback callback,
                               const std::string& machine_id) {
  std::string storage_id_key = chrome::GetCdmStorageIdKey();
  std::move(callback).Run(
      ComputeStorageId(storage_id_key, profile_salt, origin, machine_id));
}
#endif  // defined(OS_CHROMEOS)

#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

}  // namespace

void ComputeStorageId(const std::vector<uint8_t>& salt,
                      const url::Origin& origin,
                      CdmStorageIdCallback callback) {
#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)

#if defined(OS_WIN) || defined(OS_MACOSX)
  std::string machine_id;
  std::string storage_id_key = chrome::GetCdmStorageIdKey();
  rlz_lib::GetMachineId(&machine_id);
  std::move(callback).Run(
      ComputeStorageId(storage_id_key, salt, origin, machine_id));

#elif defined(OS_CHROMEOS)
  CdmStorageIdCallback scoped_callback =
      media::ScopedCallbackRunner(std::move(callback), std::vector<uint8_t>());
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ComputeAndReturnStorageId, salt, origin,
                 base::Passed(&scoped_callback)));

#else
#error Storage ID enabled but not implemented for this platform.
#endif

#else
  // CDM Storage ID not enabled.
  std::move(callback).Run(std::vector<uint8_t>());
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)
}

}  // namespace cdm_storage_id
