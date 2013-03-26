// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cocoa_spelling_engine_mac.h"

#include "chrome/common/spellcheck_messages.h"
#include "content/public/renderer/render_thread.h"

using content::RenderThread;

SpellingEngine* CreateNativeSpellingEngine() {
  return new CocoaSpellingEngine();
}

void CocoaSpellingEngine::Init(base::PlatformFile bdict_file) {
  DCHECK(bdict_file == base::kInvalidPlatformFileValue);
}

bool CocoaSpellingEngine::InitializeIfNeeded() {
  return false;  // We never need to initialize.
}

bool CocoaSpellingEngine::IsEnabled() {
  return true;  // OSX is always enabled.
}

// Synchronously query against NSSpellCheck.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
bool CocoaSpellingEngine::CheckSpelling(const string16& word_to_check,
                                        int tag) {
  bool word_correct = false;
  RenderThread::Get()->Send(new SpellCheckHostMsg_CheckSpelling(
      word_to_check, tag, &word_correct));
  return word_correct;
}

// Synchronously query against NSSpellCheck.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
void CocoaSpellingEngine::FillSuggestionList(
    const string16& wrong_word,
    std::vector<string16>* optional_suggestions) {
    RenderThread::Get()->Send(new SpellCheckHostMsg_FillSuggestionList(
        wrong_word, optional_suggestions));
}
