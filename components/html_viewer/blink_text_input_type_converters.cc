// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_text_input_type_converters.h"

#include "base/basictypes.h"
#include "base/macros.h"

namespace mojo {

#define TEXT_INPUT_TYPE_ASSERT(NAME, Name) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_TYPE_##NAME) == \
                 static_cast<int32>(blink::WebTextInputType##Name), \
                 text_input_type_should_match)
TEXT_INPUT_TYPE_ASSERT(NONE, None);
TEXT_INPUT_TYPE_ASSERT(TEXT, Text);
TEXT_INPUT_TYPE_ASSERT(PASSWORD, Password);
TEXT_INPUT_TYPE_ASSERT(SEARCH, Search);
TEXT_INPUT_TYPE_ASSERT(EMAIL, Email);
TEXT_INPUT_TYPE_ASSERT(NUMBER, Number);
TEXT_INPUT_TYPE_ASSERT(TELEPHONE, Telephone);
TEXT_INPUT_TYPE_ASSERT(URL, URL);
TEXT_INPUT_TYPE_ASSERT(DATE, Date);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME, DateTime);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME_LOCAL, DateTimeLocal);
TEXT_INPUT_TYPE_ASSERT(MONTH, Month);
TEXT_INPUT_TYPE_ASSERT(TIME, Time);
TEXT_INPUT_TYPE_ASSERT(WEEK, Week);
TEXT_INPUT_TYPE_ASSERT(TEXT_AREA, TextArea);

#define TEXT_INPUT_FLAG_ASSERT(NAME, Name) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_FLAG_##NAME) == \
                 static_cast<int32>(blink::WebTextInputFlag##Name), \
                 text_input_flag_should_match)
TEXT_INPUT_FLAG_ASSERT(NONE, None);
TEXT_INPUT_FLAG_ASSERT(AUTOCOMPLETE_ON, AutocompleteOn);
TEXT_INPUT_FLAG_ASSERT(AUTOCOMPLETE_OFF, AutocompleteOff);
TEXT_INPUT_FLAG_ASSERT(AUTOCORRECT_ON, AutocorrectOn);
TEXT_INPUT_FLAG_ASSERT(AUTOCORRECT_OFF, AutocorrectOff);
TEXT_INPUT_FLAG_ASSERT(SPELLCHECK_ON, SpellcheckOn);
TEXT_INPUT_FLAG_ASSERT(SPELLCHECK_OFF, SpellcheckOff);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_NONE, AutocapitalizeNone);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_CHARACTERS, AutocapitalizeCharacters);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_WORDS, AutocapitalizeWords);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_SENTENCES, AutocapitalizeSentences);

TextInputType
TypeConverter<TextInputType, blink::WebTextInputType>::Convert(
    const blink::WebTextInputType& input) {
  return static_cast<TextInputType>(input);
}

}  // namespace mojo
