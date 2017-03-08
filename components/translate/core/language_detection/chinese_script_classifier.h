// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_

#include <memory>
#include <string>
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace translate {

class ChineseScriptClassifier {
 public:
  // Initializes both the Hant-to-Hans ICU transliterator and the
  // Hans-to-Hant ICU transliterator.
  ChineseScriptClassifier();
  ~ChineseScriptClassifier();

  // Given Chinese text as input, returns either zh-Hant or zh-Hans.
  // When the input is ambiguous, i.e. not completely zh-Hans and not
  // completely zh-Hant, this function returns the closest language code
  // matching the input.
  //
  // Behavior is undefined for non-Chinese input.
  std::string Classify(const std::string& input) const;

  // Returns true if the underlying transliterators were properly initialized
  // by the constructor.
  bool IsInitialized() const;

 private:
  // ICU Transliterator that does Hans to Hant conversion.
  std::unique_ptr<icu::Transliterator> hans2hant_;

  // ICU Transliterator that does Hant to Hans conversion.
  std::unique_ptr<icu::Transliterator> hant2hans_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_
