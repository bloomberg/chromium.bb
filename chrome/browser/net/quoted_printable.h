// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_QUOTED_PRINTABLE_H_
#define CHROME_BROWSER_NET_QUOTED_PRINTABLE_H_
#pragma once

#include <string>

// Some functions to encode/decode with the quoted-printable encoding.
// See http://tools.ietf.org/html/rfc2045#section-6.7

namespace chrome {
namespace browser {
namespace net {

// Encodes the input string with the quoted-printable encoding.
void QuotedPrintableEncode(const std::string& input, std::string* output);

// Decodes the quoted-printable input string.  Returns true if the input string
// was wellformed quoted-printable, false otherwise, in which case it still
// decodes as much of the message as possible.
bool QuotedPrintableDecode(const std::string& input, std::string* output);

// Returns 0 if |iter| does not point to an end-of-line, the number of chars
// that constitutes that EOL otherwise (1 for LF, 2 for CR-LF).
// Exposed as it is also used in unit-tests.
int IsEOL(const std::string::const_iterator& iter, const std::string& input);

}  // namespace net
}  // namespace browser
}  // namespace chrome

#endif  // CHROME_BROWSER_NET_QUOTED_PRINTABLE_H_
