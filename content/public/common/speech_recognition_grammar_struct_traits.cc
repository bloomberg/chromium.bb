// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/speech_recognition_grammar_struct_traits.h"
#include <string>

namespace mojo {

// static
bool StructTraits<content::mojom::SpeechRecognitionGrammarDataView,
                  content::SpeechRecognitionGrammar>::
    Read(content::mojom::SpeechRecognitionGrammarDataView data,
         content::SpeechRecognitionGrammar* out) {
  if (!data.ReadUrl(&out->url))
    return false;
  out->weight = data.weight();
  return true;
}

}  // namespace mojo
