// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/chinese_script_classifier.h"

#include <algorithm>
#include <memory>
#include <string>
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace translate {

namespace {
// BCP 47 language code representing Chinese in Han Simplified script.
const char kChineseSimplifiedCode[] = "zh-Hans";

// BCP 47 language code representing Chinese in Han Traditional script.
const char kChineseTraditionalCode[] = "zh-Hant";
}  // namespace

ChineseScriptClassifier::ChineseScriptClassifier() {
  UParseError parse_status;
  UErrorCode status = U_ZERO_ERROR;
  // The Transliterator IDs are defined in:
  // third_party/icu/source/data/translit/root.txt.
  //
  // Chromium keeps only a subset of these, defined in:
  // third_party/icu/source/data/translit/root_subset.txt
  hans2hant_.reset(icu::Transliterator::createInstance(
      icu::UnicodeString("Hans-Hant"), UTRANS_FORWARD, parse_status, status));
  DVLOG(1) << "Hans-Hant Transliterator initialization status: "
           << u_errorName(status);
  hant2hans_.reset(icu::Transliterator::createInstance(
      icu::UnicodeString("Hant-Hans"), UTRANS_FORWARD, parse_status, status));
  DVLOG(1) << "Hant-Hans Transliterator initialization status: "
           << u_errorName(status);
}

bool ChineseScriptClassifier::IsInitialized() const {
  return hans2hant_ && hant2hans_;
}

ChineseScriptClassifier::~ChineseScriptClassifier() {}

std::string ChineseScriptClassifier::Classify(const std::string& input) const {
  // If there was a problem with initialization, return the empty string.
  if (!IsInitialized()) {
    return "";
  }

  // Operate only on first 500 bytes.
  std::string input_subset;
  base::TruncateUTF8ToByteSize(input, 500, &input_subset);

  // Remove whitespace since transliterators may not preserve it.
  input_subset.erase(std::remove_if(input_subset.begin(), input_subset.end(),
                                    base::IsUnicodeWhitespace),
                     input_subset.end());

  // Convert two copies of the input to icu::UnicodeString. Two copies are
  // necessary because transliteration happens in place only.
  icu::UnicodeString original_input =
      icu::UnicodeString::fromUTF8(input_subset);
  icu::UnicodeString hant_input = icu::UnicodeString::fromUTF8(input_subset);
  icu::UnicodeString hans_input = icu::UnicodeString::fromUTF8(input_subset);

  // Get the zh-Hant version of this input.
  hans2hant_->transliterate(hant_input);
  // Get the zh-Hans version of this input.
  hant2hans_->transliterate(hans_input);

  // Debugging only: show the input, the Hant version, and the Hans version.
  if (VLOG_IS_ON(1)) {
    std::string hant_string;
    std::string hans_string;
    hans_input.toUTF8String(hans_string);
    hant_input.toUTF8String(hant_string);
    DVLOG(1) << "Original input:\n" << input_subset;
    DVLOG(1) << "zh-Hant output:\n" << hant_string;
    DVLOG(1) << "zh-Hans output:\n" << hans_string;
  }

  // Count matches between the original input chars and the Hant and Hans
  // versions of the input.
  int hant_count = 0;
  int hans_count = 0;

  // Iterate over all chars in the original input and compute matches between
  // the Hant version and the Hans version.
  //
  // All segments (original, Hant, and Hans) should have the same length, but
  // in case of some corner case or bug in which they turn out not to be,
  // we compute the minimum length we are allowed to traverse.
  const int min_length =
      std::min(original_input.length(),
               std::min(hans_input.length(), hant_input.length()));
  for (int index = 0; index < min_length; ++index) {
    const auto original_char = original_input.charAt(index);
    const auto hans_char = hans_input.charAt(index);
    const auto hant_char = hant_input.charAt(index);
    if (hans_char == hant_char) {
      continue;
    } else if (original_char == hans_char) {
      // Hans-specific char found.
      ++hans_count;
    } else if (original_char == hant_char) {
      // Hant-specific char found.
      ++hant_count;
    }
  }
  DVLOG(1) << "Found " << hans_count << " zh-Hans chars in input";
  DVLOG(1) << "Found " << hant_count << " zh-Hant chars in input";

  if (hant_count > hans_count) {
    return kChineseTraditionalCode;
  } else if (hans_count > hant_count) {
    return kChineseSimplifiedCode;
  } else {  // hans_count == hant_count
    // All characters are the same in both scripts. In this case, we return the
    // following code.
    return kChineseSimplifiedCode;
  }
}

}  // namespace translate
