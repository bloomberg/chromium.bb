// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_STRUCT_TRAITS_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_STRUCT_TRAITS_H_

#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_grammar.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::SpeechRecognitionGrammarDataView,
                    content::SpeechRecognitionGrammar> {
  static const std::string& url(const content::SpeechRecognitionGrammar& r) {
    return r.url;
  }
  static double weight(const content::SpeechRecognitionGrammar& r) {
    return r.weight;
  }
  static bool Read(content::mojom::SpeechRecognitionGrammarDataView data,
                   content::SpeechRecognitionGrammar* out);
};

}  // namespace mojo

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_STRUCT_TRAITS_H_
