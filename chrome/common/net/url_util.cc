// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/url_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chrome_common_net {

void WriteURLToClipboard(const GURL& url, const std::string& languages) {
  if (url.is_empty() || !url.is_valid())
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  base::string16 text =
      url.SchemeIs(url::kMailToScheme)
          ? base::ASCIIToUTF16(url.path())
          : url_formatter::FormatUrl(
                url, languages, url_formatter::kFormatUrlOmitNothing,
                net::UnescapeRule::NONE, nullptr, nullptr, nullptr);

  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  scw.WriteURL(text);
}

}  // namespace chrome_common_net
