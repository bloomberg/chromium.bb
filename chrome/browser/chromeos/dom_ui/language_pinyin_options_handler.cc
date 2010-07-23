// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_pinyin_options_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

LanguagePinyinOptionsHandler::LanguagePinyinOptionsHandler() {
}

LanguagePinyinOptionsHandler::~LanguagePinyinOptionsHandler() {
}

void LanguagePinyinOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Pinyin page - ChromeOS
}
