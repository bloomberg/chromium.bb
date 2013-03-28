// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/util.h"

#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/key_converter.h"

std::string GenerateId() {
  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

namespace {

Status FlattenStringArray(const base::ListValue* src, string16* dest) {
  string16 keys;
  for (size_t i = 0; i < src->GetSize(); ++i) {
    string16 keys_list_part;
    if (!src->GetString(i, &keys_list_part))
      return Status(kUnknownError, "keys should be a string");
    for (size_t j = 0; j < keys_list_part.size(); ++j) {
      if (CBU16_IS_SURROGATE(keys_list_part[j])) {
        return Status(kUnknownError,
                      "ChromeDriver only supports characters in the BMP");
      }
    }
    keys.append(keys_list_part);
  }
  *dest = keys;
  return Status(kOk);
}

}  // namespace

Status SendKeysOnWindow(
    WebView* web_view,
    const base::ListValue* key_list,
    bool release_modifiers,
    int* sticky_modifiers) {
  string16 keys;
  Status status = FlattenStringArray(key_list, &keys);
  if (status.IsError())
    return status;
  std::list<KeyEvent> events;
  int sticky_modifiers_tmp = *sticky_modifiers;
  status = ConvertKeysToKeyEvents(
      keys, release_modifiers, &sticky_modifiers_tmp, &events);
  if (status.IsError())
    return status;
  status = web_view->DispatchKeyEvents(events);
  if (status.IsOk())
    *sticky_modifiers = sticky_modifiers_tmp;
  return status;
}
