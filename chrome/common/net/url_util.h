// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_UTIL_H_
#define CHROME_COMMON_NET_URL_UTIL_H_

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

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_URL_UTIL_H_
