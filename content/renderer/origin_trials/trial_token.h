// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_H_
#define CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// The Experimental Framework (EF) provides limited access to experimental
// features, on a per-origin basis (origin trials). This class defines the trial
// token data structure, used to securely provide access to an experimental
// feature.
//
// Features are defined by string names, provided by the implementers. The EF
// code does not maintain an enum or constant list for feature names. Instead,
// the EF validates the name provided by the feature implementation against any
// provided tokens.
//
// More documentation on the token format can be found at
// https://docs.google.com/document/d/1v5fi0EUV_QHckVHVF2K4P72iNywnrJtNhNZ6i2NPt0M

class CONTENT_EXPORT TrialToken {
 public:
  ~TrialToken();

  // Returns a token object if the string represents a well-formed token, or
  // nullptr otherwise. (This does not mean that the token is valid, just that
  // it can be parsed.)
  static scoped_ptr<TrialToken> Parse(const std::string& token_text);

  // Returns true if this feature is appropriate for use by the given origin,
  // for the given feature name. This does not check whether the signature is
  // valid, or whether the token itself has expired.
  bool IsAppropriate(const std::string& origin,
                     const std::string& featureName) const;

  // Returns true if this token has a valid signature, and has not expired.
  bool IsValid(const base::Time& now) const;

  std::string signature() { return signature_; }
  std::string data() { return data_; }
  GURL origin() { return origin_; }
  std::string feature_name() { return feature_name_; }
  uint64_t expiry_timestamp() { return expiry_timestamp_; }

 protected:
  friend class TrialTokenTest;

  bool ValidateOrigin(const std::string& origin) const;
  bool ValidateFeatureName(const std::string& feature_name) const;
  bool ValidateDate(const base::Time& now) const;
  bool ValidateSignature(const base::StringPiece& public_key) const;

  static bool ValidateSignature(const std::string& signature_text,
                                const std::string& data,
                                const base::StringPiece& public_key);

 private:
  TrialToken(const std::string& signature,
             const std::string& data,
             const GURL& origin,
             const std::string& feature_name,
             uint64_t expiry_timestamp);

  // The base64-encoded-signature portion of the token. For the token to be
  // valid, this must be a valid signature for the data portion of the token, as
  // verified by the public key in trial_token.cc.
  std::string signature_;

  // The portion of the token string which is signed, and whose signature is
  // contained in the signature_ member.
  std::string data_;

  // The origin for which this token is valid. Must be a secure origin.
  GURL origin_;

  // The name of the experimental feature which this token enables.
  std::string feature_name_;

  // The time until which this token should be considered valid, in UTC, as
  // seconds since the Unix epoch.
  uint64_t expiry_timestamp_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_H_
