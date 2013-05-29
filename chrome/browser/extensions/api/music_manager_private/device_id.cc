// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#if defined(OS_CHROMEOS)
#include "chromeos/cryptohome/cryptohome_library.h"
#endif
#include "crypto/hmac.h"
#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace {

// Return a unique identifier for the machine/device. Currently, this is only
// supported on Windows (with RLZ enabled) and ChromeOS.
std::string GetMachineID() {
#if defined(OS_WIN) && defined(ENABLE_RLZ)
  std::string result;
  rlz_lib::GetMachineId(&result);
  return result;
#elif defined(OS_CHROMEOS)
  chromeos::CryptohomeLibrary* c_home = chromeos::CryptohomeLibrary::Get();
  return c_home->GetSystemSalt();
#else
  // Not implemented for other platforms.
  return "";
#endif
}

// Compute HMAC-SHA256(|key|, |text|) as a string.
bool ComputeHmacSha256(const std::string& key,
                       const std::string& text,
                       std::string* signature_return) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8> digest(digest_length);
  bool result = hmac.Init(key) &&
      hmac.Sign(text, &digest[0], digest.size()) &&
      base::Base64Encode(std::string(reinterpret_cast<char*>(&digest[0]),
                                     digest.size()),
                         signature_return);
  return result;
}

}

namespace device_id {

std::string GetDeviceID(const std::string& salt) {
  CHECK(!salt.empty());
  std::string machine_id = GetMachineID();
  if (machine_id.empty()) {
    DLOG(WARNING) << "API is not supported on current platform.";
    return "";
  }

  std::string device_id;
  if (!ComputeHmacSha256(machine_id, salt, &device_id)) {
    DLOG(ERROR) << "Error while computing HMAC-SHA256 of device id.";
    return "";
  }
  return device_id;
}

}  // namespace device_id
