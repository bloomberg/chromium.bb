// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/url_util.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace chrome_common_net {

void WriteURLToClipboard(const GURL& url,
                         const std::string& languages,
                         ui::Clipboard *clipboard,
                         ui::Clipboard::SourceTag source_tag) {
  if (url.is_empty() || !url.is_valid() || !clipboard)
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  string16 text = url.SchemeIs(chrome::kMailToScheme) ?
      ASCIIToUTF16(url.path()) :
      net::FormatUrl(url, languages, net::kFormatUrlOmitNothing,
                     net::UnescapeRule::NONE, NULL, NULL, NULL);

  ui::ScopedClipboardWriter scw(clipboard,
                                ui::Clipboard::BUFFER_STANDARD,
                                source_tag);
  scw.WriteURL(text);
}

}  // namespace chrome_common_net
