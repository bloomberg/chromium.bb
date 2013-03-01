// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_UTIL_H_
#define CHROME_COMMON_NET_URL_UTIL_H_

#include <string>

#include <ui/base/clipboard/clipboard.h>

class GURL;

namespace chrome_common_net {

// Writes a string representation of |url| to the system clipboard.
void WriteURLToClipboard(const GURL& url,
                         const std::string& languages,
                         ui::Clipboard *clipboard,
                         ui::Clipboard::SourceTag source_tag);

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_URL_UTIL_H_
