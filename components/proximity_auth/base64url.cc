// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/base64url.h"

#include "base/base64.h"
#include "base/strings/string_util.h"

namespace proximity_auth {

void Base64UrlEncode(const std::string& decoded_input,
                     std::string* encoded_output) {
  base::Base64Encode(decoded_input, encoded_output);
  base::ReplaceChars(*encoded_output, "+", "-", encoded_output);
  base::ReplaceChars(*encoded_output, "/", "_", encoded_output);
}

bool Base64UrlDecode(const std::string& encoded_input,
                     std::string* decoded_output) {
  std::string adjusted_encoded_input = encoded_input;
  base::ReplaceChars(adjusted_encoded_input, "-", "+", &adjusted_encoded_input);
  base::ReplaceChars(adjusted_encoded_input, "_", "/", &adjusted_encoded_input);

  return base::Base64Decode(adjusted_encoded_input, decoded_output);
}

}  // namespace proximity_auth
