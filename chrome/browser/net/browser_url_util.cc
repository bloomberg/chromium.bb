// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/browser_url_util.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace chrome_browser_net {

void WriteURLToClipboard(const GURL& url,
                         const std::string& languages,
                         ui::Clipboard *clipboard) {
  if (url.is_empty() || !url.is_valid() || !clipboard)
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  string16 text = url.SchemeIs(chrome::kMailToScheme) ?
      ASCIIToUTF16(url.path()) :
      net::FormatUrl(url, languages, net::kFormatUrlOmitNothing,
                     net::UnescapeRule::NONE, NULL, NULL, NULL);

  ui::ScopedClipboardWriter scw(clipboard);
  scw.WriteURL(text);
}

GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());
  if (!query.empty())
    query += "&";
  query += (net::EscapeQueryParamValue(name, true) + "=" +
            net::EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

bool GetValueForKeyInQuery(const GURL& url,
                           const std::string& search_key,
                           std::string* out_value) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(
      url.spec().c_str(), &query, &key, &value)) {
    if (key.is_nonempty()) {
      std::string key_string = url.spec().substr(key.begin, key.len);
      if (key_string == search_key) {
        if (value.is_nonempty()) {
          *out_value = net::UnescapeURLComponent(
              url.spec().substr(value.begin, value.len),
              net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
        } else {
          *out_value = "";
        }
        return true;
      }
    }
  }
  return false;
}

}  // namespace chrome_browser_net
