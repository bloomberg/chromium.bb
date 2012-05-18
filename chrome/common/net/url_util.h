// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_UTIL_H_
#define CHROME_COMMON_NET_URL_UTIL_H_
#pragma once

#include <string>

class GURL;

namespace ui {
class Clipboard;
}

namespace chrome_common_net {

// Writes a string representation of |url| to the system clipboard.
void WriteURLToClipboard(const GURL& url,
                         const std::string& languages,
                         ui::Clipboard *clipboard);

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value);

// Returns a new GURL by appending or replacing the given query parameter name
// and the value. If |name| appears more than once, only the first name-value
// pair is replaced. Unsafe characters in the name and the value are escaped
// like %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com"), "name", "new").spec()
// => "http://example.com?name=value"
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com?x=y&name=old"), "name", "new").spec()
// => "http://example.com?x=y&name=new"
GURL AppendOrReplaceQueryParameter(const GURL& url,
                                   const std::string& name,
                                   const std::string& value);

// Looks for |search_key| in the query portion of |url|. Returns true if the
// key is found and sets |out_value| to the unescaped value for the key.
// Returns false if the key is not found.
bool GetValueForKeyInQuery(const GURL& url,
                           const std::string& search_key,
                           std::string* out_value);

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_URL_UTIL_H_
