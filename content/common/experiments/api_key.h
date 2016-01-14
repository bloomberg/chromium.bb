// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_EXPERIMENTS_API_KEY_H_
#define CONTENT_COMMON_EXPERIMENTS_API_KEY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// The Experimental Framework (EF) provides limited access to experimental APIs,
// on a per-origin basis.  This class defines the API key data structure, used
// to securely provide access to an experimental API.
//
// Experimental APIs are defined by string names, provided by the implementers.
// The EF code does not maintain an enum or constant list for experiment names.
// Instead, the EF validates the name provided by the API implementation against
// any provided API keys.
//
// More documentation on the key format can be found at
// https://docs.google.com/document/d/1v5fi0EUV_QHckVHVF2K4P72iNywnrJtNhNZ6i2NPt0M

class CONTENT_EXPORT ApiKey {
 public:
  ~ApiKey();

  // Returns a key object if the string represents a well-formed key, or
  // nullptr otherwise. (This does not mean that the key is valid, just that it
  // can be parsed.)
  static scoped_ptr<ApiKey> Parse(const std::string& key_text);

  // Returns true if this API is appropriate for use by the given origin, for
  // the given API name. This does not check whether the signature is valid, or
  // whether the key itself has expired.
  bool IsAppropriate(const std::string& origin,
                     const std::string& apiName) const;

  // Returns true if this API key has a valid signature, and has not expired.
  bool IsValid(const base::Time& now) const;

  std::string signature() { return signature_; }
  std::string data() { return data_; }
  GURL origin() { return origin_; }
  std::string api_name() { return api_name_; }
  uint64_t expiry_timestamp() { return expiry_timestamp_; }

 protected:
  friend class ApiKeyTest;

  bool ValidateOrigin(const std::string& origin) const;
  bool ValidateApiName(const std::string& api_name) const;
  bool ValidateDate(const base::Time& now) const;
  bool ValidateSignature(const base::StringPiece& public_key) const;

  static bool ValidateSignature(const std::string& signature_text,
                                const std::string& data,
                                const base::StringPiece& public_key);

 private:
  ApiKey(const std::string& signature,
         const std::string& data,
         const GURL& origin,
         const std::string& api_name,
         uint64_t expiry_timestamp);

  // The base64-encoded-signature portion of the key. For the key to be valid,
  // this must be a valid signature for the data portion of the key, as verified
  // by the public key in api_key.cc.
  std::string signature_;

  // The portion of the key string which is signed, and whose signature is
  // contained in the signature_ member.
  std::string data_;

  // The origin for which this key is valid. Must be a secure origin.
  GURL origin_;

  // The name of the API experiment which this key enables.
  std::string api_name_;

  // The time until which this key should be considered valid, in UTC, as
  // seconds since the Unix epoch.
  uint64_t expiry_timestamp_;
};

}  // namespace content

#endif  // CONTENT_COMMON_EXPERIMENTS_API_KEY_H_
