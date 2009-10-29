// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/browser_url_util.h"

#include "app/clipboard/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace chrome_browser_net {

void WriteURLToClipboard(const GURL& url,
                         const std::wstring& languages,
                         Clipboard *clipboard) {
  if (url.is_empty() || !url.is_valid() || !clipboard)
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  string16 text = url.SchemeIs(chrome::kMailToScheme) ?
                      ASCIIToUTF16(url.path()) :
                      WideToUTF16(net::FormatUrl(url, languages, false,
                                             UnescapeRule::NONE, NULL, NULL));

  ScopedClipboardWriter scw(clipboard);
  scw.WriteURL(text);
}

}  // namespace chrome_browser_net
