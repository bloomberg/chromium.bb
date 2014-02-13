// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/media_device_id.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/hmac.h"

namespace content {

std::string GetHMACForMediaDeviceID(const ResourceContext::SaltCallback& sc,
                                    const GURL& security_origin,
                                    const std::string& raw_unique_id) {
  DCHECK(security_origin.is_valid());
  DCHECK(!raw_unique_id.empty());
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8> digest(digest_length);
  std::string salt = sc.Run();
  bool result = hmac.Init(security_origin.spec()) &&
      hmac.Sign(raw_unique_id + salt, &digest[0], digest.size());
  DCHECK(result);
  return StringToLowerASCII(base::HexEncode(&digest[0], digest.size()));
}

bool DoesMediaDeviceIDMatchHMAC(const ResourceContext::SaltCallback& sc,
                                const GURL& security_origin,
                                const std::string& device_guid,
                                const std::string& raw_unique_id) {
  DCHECK(security_origin.is_valid());
  DCHECK(!raw_unique_id.empty());
  std::string guid_from_raw_device_id =
      GetHMACForMediaDeviceID(sc, security_origin, raw_unique_id);
  return guid_from_raw_device_id == device_guid;
}

}  // namespace content
