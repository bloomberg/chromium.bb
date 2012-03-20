// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

namespace content {

void WebUIMessageHandler::SetURLAndTitle(DictionaryValue* dictionary,
                                         string16 title,
                                         const GURL& gurl) {
  dictionary->SetString("url", gurl.spec());

  bool using_url_as_the_title = false;
  if (title.empty()) {
    using_url_as_the_title = true;
    title = UTF8ToUTF16(gurl.spec());
  }

  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings.
  string16 title_to_set(title);
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title) {
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      base::i18n::AdjustStringForLocaleDirection(&title_to_set);
    }
  }
  dictionary->SetString("title", title_to_set);
}

bool WebUIMessageHandler::ExtractIntegerValue(const ListValue* value,
                                              int* out_int) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToInt(string_value, out_int);
  double double_value;
  if (value->GetDouble(0, &double_value)) {
    *out_int = static_cast<int>(double_value);
    return true;
  }
  NOTREACHED();
  return false;
}

bool WebUIMessageHandler::ExtractDoubleValue(const ListValue* value,
                                             double* out_value) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToDouble(string_value, out_value);
  if (value->GetDouble(0, out_value))
    return true;
  NOTREACHED();
  return false;
}

string16 WebUIMessageHandler::ExtractStringValue(const ListValue* value) {
  string16 string16_value;
  if (value->GetString(0, &string16_value))
    return string16_value;
  NOTREACHED();
  return string16();
}

}  // namespace content
