// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hmac.h"

namespace {

// Compute HMAC-SHA256(|key|, |text|) as a string.
bool ComputeHmacSha256(const std::string& key,
                       const std::string& text,
                       std::string* signature_return) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8> digest(digest_length);
  bool result = hmac.Init(key) &&
      hmac.Sign(text, &digest[0], digest.size());
  if (result) {
    *signature_return = StringToLowerASCII(base::HexEncode(digest.data(),
                                                           digest.size()));
  }
  return result;
}

void GetMachineIdCallback(const std::string& extension_id,
                          const extensions::api::DeviceId::IdCallback& callback,
                          const std::string& machine_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (machine_id.empty()) {
    callback.Run("");
  }

  std::string device_id;
  if (!ComputeHmacSha256(machine_id, extension_id, &device_id)) {
    DLOG(ERROR) << "Error while computing HMAC-SHA256 of device id.";
    callback.Run("");
  }
  callback.Run(device_id);
}

}

namespace extensions {
namespace api {

/* static */
void DeviceId::GetDeviceId(const std::string& extension_id,
                           const IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!extension_id.empty());

  // Forward call to platform specific implementation, then compute the HMAC
  // in the callback.
  GetMachineId(base::Bind(&GetMachineIdCallback, extension_id, callback));
}

}  // namespace api
}  // namespace extensions
