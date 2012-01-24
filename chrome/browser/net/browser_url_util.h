// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_BROWSER_URL_UTIL_H_
#define CHROME_BROWSER_NET_BROWSER_URL_UTIL_H_
#pragma once

#include <string>

class GURL;

namespace ui {
class Clipboard;
}

namespace chrome_browser_net {

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

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_BROWSER_URL_UTIL_H_
