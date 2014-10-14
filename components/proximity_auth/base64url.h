// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_BASE64URL_H
#define COMPONENTS_PROXIMITY_BASE64URL_H

#include <string>

namespace proximity_auth {

// An implmentation of the base64url encoding. Escapes the unsafe '+' and '/'
// characters with the url-safe alternatives '-' and '_', respectively. For more
// info, see the section on "Base 64 Encoding with URL and Filename Safe
// Alphabet" in http://www.ietf.org/rfc/rfc4648.txt
// NOTE: Unlike many common web-safe encodings, this function does not escape
// the '=' character. This is to match the expectations set by Android.
// TODO(isherman): There are several (semantically slightly different)
// implementations of this within the Chromium codebase. Try to unify them.
void Base64UrlEncode(const std::string& decoded_input,
                     std::string* encoded_output);

// The inverse operation for the base64url encoding above.
bool Base64UrlDecode(const std::string& encoded_input,
                     std::string* decoded_output);

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_BASE64URL_H
