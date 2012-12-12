// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

namespace content {

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
