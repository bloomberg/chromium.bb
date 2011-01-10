// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_BROWSER_URL_UTIL_H_
