// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/common/translate_struct_traits.h"

#include "ipc/ipc_message_utils.h"
#include "url/mojo/url_gurl_struct_traits.h"

using namespace translate;

namespace mojo {

mojom::TranslateError
EnumTraits<mojom::TranslateError, TranslateErrors::Type>::ToMojom(
    TranslateErrors::Type input) {
  switch (input) {
    case TranslateErrors::Type::NONE:
      return mojom::TranslateError::NONE;
    case TranslateErrors::Type::NETWORK:
      return mojom::TranslateError::NETWORK;
    case TranslateErrors::Type::INITIALIZATION_ERROR:
      return mojom::TranslateError::INITIALIZATION_ERROR;
    case TranslateErrors::Type::UNKNOWN_LANGUAGE:
      return mojom::TranslateError::UNKNOWN_LANGUAGE;
    case TranslateErrors::Type::UNSUPPORTED_LANGUAGE:
      return mojom::TranslateError::UNSUPPORTED_LANGUAGE;
    case TranslateErrors::Type::IDENTICAL_LANGUAGES:
      return mojom::TranslateError::IDENTICAL_LANGUAGES;
    case TranslateErrors::Type::TRANSLATION_ERROR:
      return mojom::TranslateError::TRANSLATION_ERROR;
    case TranslateErrors::Type::TRANSLATION_TIMEOUT:
      return mojom::TranslateError::TRANSLATION_TIMEOUT;
    case TranslateErrors::Type::UNEXPECTED_SCRIPT_ERROR:
      return mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR;
    case TranslateErrors::Type::BAD_ORIGIN:
      return mojom::TranslateError::BAD_ORIGIN;
    case TranslateErrors::Type::SCRIPT_LOAD_ERROR:
      return mojom::TranslateError::SCRIPT_LOAD_ERROR;
    case TranslateErrors::Type::TRANSLATE_ERROR_MAX:
      return mojom::TranslateError::TRANSLATE_ERROR_MAX;
  }

  NOTREACHED();
  return mojom::TranslateError::NONE;
}

bool EnumTraits<mojom::TranslateError, TranslateErrors::Type>::FromMojom(
    mojom::TranslateError input,
    TranslateErrors::Type* output) {
  switch (input) {
    case mojom::TranslateError::NONE:
      *output = TranslateErrors::Type::NONE;
      return true;
    case mojom::TranslateError::NETWORK:
      *output = TranslateErrors::Type::NETWORK;
      return true;
    case mojom::TranslateError::INITIALIZATION_ERROR:
      *output = TranslateErrors::Type::INITIALIZATION_ERROR;
      return true;
    case mojom::TranslateError::UNKNOWN_LANGUAGE:
      *output = TranslateErrors::Type::UNKNOWN_LANGUAGE;
      return true;
    case mojom::TranslateError::UNSUPPORTED_LANGUAGE:
      *output = TranslateErrors::Type::UNSUPPORTED_LANGUAGE;
      return true;
    case mojom::TranslateError::IDENTICAL_LANGUAGES:
      *output = TranslateErrors::Type::IDENTICAL_LANGUAGES;
      return true;
    case mojom::TranslateError::TRANSLATION_ERROR:
      *output = TranslateErrors::Type::TRANSLATION_ERROR;
      return true;
    case mojom::TranslateError::TRANSLATION_TIMEOUT:
      *output = TranslateErrors::Type::TRANSLATION_TIMEOUT;
      return true;
    case mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR:
      *output = TranslateErrors::Type::UNEXPECTED_SCRIPT_ERROR;
      return true;
    case mojom::TranslateError::BAD_ORIGIN:
      *output = TranslateErrors::Type::BAD_ORIGIN;
      return true;
    case mojom::TranslateError::SCRIPT_LOAD_ERROR:
      *output = TranslateErrors::Type::SCRIPT_LOAD_ERROR;
      return true;
    case mojom::TranslateError::TRANSLATE_ERROR_MAX:
      *output = TranslateErrors::Type::TRANSLATE_ERROR_MAX;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<mojom::LanguageDetectionDetailsDataView,
                  LanguageDetectionDetails>::
    Read(mojom::LanguageDetectionDetailsDataView data,
         LanguageDetectionDetails* out) {
  if (!data.ReadTime(&out->time))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadContentLanguage(&out->content_language))
    return false;
  if (!data.ReadCldLanguage(&out->cld_language))
    return false;

  out->is_cld_reliable = data.is_cld_reliable();
  out->has_notranslate = data.has_notranslate();

  if (!data.ReadHtmlRootLanguage(&out->html_root_language))
    return false;
  if (!data.ReadAdoptedLanguage(&out->adopted_language))
    return false;
  if (!data.ReadContents(&out->contents))
    return false;

  return true;
}

}  // namespace mojo
