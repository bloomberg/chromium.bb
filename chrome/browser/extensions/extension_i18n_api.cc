// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_i18n_api.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

// Errors.
static const char kEmptyAcceptLanguagesError[] = "accept-languages is empty.";

bool GetAcceptLanguagesFunction::RunImpl() {
  string16 acceptLanguages =
      UTF8ToUTF16(profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  // Currently, there are 2 ways to set browser's accept-languages: through UI
  // or directly modify the preference file. The accept-languages set through
  // UI is guranteed to be valid, and the accept-languages string returned from
  // profile()->GetPrefs()->GetString(prefs::kAcceptLanguages) is guranteed to
  // be valid and well-formed, which means each accept-langauge is a valid
  // code, and accept-languages are seperatd by "," without surrrounding
  // spaces. But we do not do any validation (either the format or the validity
  // of the language code) on accept-languages set through editing preference
  // file directly. So, here, we're adding extra checks to be resistant to
  // crashes caused by data corruption.
  result_.reset(new ListValue());
  if (acceptLanguages.empty()) {
    error_ = kEmptyAcceptLanguagesError;
    return false;
  }
  size_t begin = 0;
  size_t end;
  while (1) {
    end = acceptLanguages.find(',', begin);
    if (end > begin) {
      // Guard against a malformed value with multiple "," in a row.
      string16 acceptLang = acceptLanguages.substr(begin, end - begin);
      static_cast<ListValue*>(result_.get())->
          Append(Value::CreateStringValue(acceptLang));
    }
    begin = end + 1;
    // 'begin >= acceptLanguages.length()' to guard against a value
    // ending with ','.
    if (end == string16::npos || begin >= acceptLanguages.length())
      break;
  }
  if (static_cast<ListValue*>(result_.get())->GetSize() == 0) {
    error_ = kEmptyAcceptLanguagesError;
    return false;
  }
  return true;
}
