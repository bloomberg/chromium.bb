// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/i18n.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "third_party/cld_2/src/public/compact_lang_det.h"
#include "third_party/cld_2/src/public/encodings.h"

class Profile;

namespace extensions {

class I18nGetAcceptLanguagesFunction : public ChromeSyncExtensionFunction {
  ~I18nGetAcceptLanguagesFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("i18n.getAcceptLanguages", I18N_GETACCEPTLANGUAGES)
};

class I18nDetectLanguageFunction : public UIThreadExtensionFunction {
 private:
  ~I18nDetectLanguageFunction() override{};

  // UIThreadExtensionFunction:
  ResponseAction Run() override;

  scoped_ptr<base::ListValue> GetLanguage(const std::string& text);
  void InitDetectedLanguages(
      CLD2::Language* langs,
      int* percent3,
      std::vector<linked_ptr<
          api::i18n::DetectLanguage::Results::Result::LanguagesType>>*
          detected_langs);

  DECLARE_EXTENSION_FUNCTION("i18n.detectLanguage", I18N_DETECTLANGUAGE)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
