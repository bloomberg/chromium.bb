// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"

#include "content/browser/android/text_suggestion_host_android.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

TextSuggestionHostMojoImplAndroid::TextSuggestionHostMojoImplAndroid(
    TextSuggestionHostAndroid* text_suggestion_host)
    : text_suggestion_host_(text_suggestion_host) {}

// static
void TextSuggestionHostMojoImplAndroid::Create(
    TextSuggestionHostAndroid* text_suggestion_host,
    blink::mojom::TextSuggestionHostRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<TextSuggestionHostMojoImplAndroid>(text_suggestion_host),
      std::move(request));
}

void TextSuggestionHostMojoImplAndroid::StartSpellCheckMenuTimer() {
  text_suggestion_host_->StartSpellCheckMenuTimer();
}

void TextSuggestionHostMojoImplAndroid::ShowSpellCheckSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    std::vector<blink::mojom::SpellCheckSuggestionPtr> suggestions) {
  text_suggestion_host_->ShowSpellCheckSuggestionMenu(caret_x, caret_y,
                                                      marked_text, suggestions);
}

}  // namespace content
