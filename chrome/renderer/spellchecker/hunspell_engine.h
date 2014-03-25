// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_
#define CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/renderer/spellchecker/spelling_engine.h"

class Hunspell;

namespace base {
class MemoryMappedFile;
}

class HunspellEngine : public SpellingEngine {
 public:
  HunspellEngine();
  virtual ~HunspellEngine();

  virtual void Init(base::File file) OVERRIDE;

  virtual bool InitializeIfNeeded() OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual bool CheckSpelling(const base::string16& word_to_check,
                             int tag) OVERRIDE;
  virtual void FillSuggestionList(
      const base::string16& wrong_word,
      std::vector<base::string16>* optional_suggestions) OVERRIDE;

 private:
  // Initializes the Hunspell dictionary, or does nothing if |hunspell_| is
  // non-null. This blocks.
  void InitializeHunspell();

  // We memory-map the BDict file.
  scoped_ptr<base::MemoryMappedFile> bdict_file_;

  // The hunspell dictionary in use.
  scoped_ptr<Hunspell> hunspell_;

  base::File file_;

  // This flag is true if hunspell is enabled.
  bool hunspell_enabled_;

  // This flag is true if we have been initialized.
  // The value indicates whether we should request a
  // dictionary from the browser when the render view asks us to check the
  // spelling of a word.
  bool initialized_;

  // This flag is true if we have requested dictionary.
  bool dictionary_requested_;
};

#endif  // CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_
