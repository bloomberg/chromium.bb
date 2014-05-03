// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKSPELLCHECK_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKSPELLCHECK_H_

#include <vector>

#include "base/basictypes.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace content {

// A mock implementation of a spell-checker used for WebKit tests.
// This class only implements the minimal functionarities required by WebKit
// tests, i.e. this class just compares the given string with known misspelled
// words in webkit tests and mark them as missspelled.
// Even though this is sufficent for webkit tests, this class is not suitable
// for any other usages.
class MockSpellCheck {
public:
    static void fillSuggestionList(const blink::WebString& word, blink::WebVector<blink::WebString>* suggestions);

    MockSpellCheck();
    ~MockSpellCheck();

    // Checks the spellings of the specified text.
    // This function returns true if the text consists of valid words, and
    // returns false if it includes invalid words.
    // When the given text includes invalid words, this function sets the
    // position of the first invalid word to misspelledOffset, and the length of
    // the first invalid word to misspelledLength, respectively.
    // For example, when the given text is "   zz zz", this function sets 3 to
    // misspelledOffset and 2 to misspelledLength, respectively.
    bool spellCheckWord(const blink::WebString& text, int* misspelledOffset, int* misspelledLength);

    // Checks whether the specified text can be spell checked immediately using
    // the spell checker cache.
    bool hasInCache(const blink::WebString& text);

    // Checks whether the specified text is a misspelling comprised of more
    // than one word. If it is, append multiple results to the results vector.
    bool isMultiWordMisspelling(const blink::WebString& text, std::vector<blink::WebTextCheckingResult>* results);

private:
    // Initialize the internal resources if we need to initialize it.
    // Initializing this object may take long time. To prevent from hurting
    // the performance of test_shell, we initialize this object when
    // SpellCheckWord() is called for the first time.
    // To be compliant with SpellCheck:InitializeIfNeeded(), this function
    // returns true if this object is downloading a dictionary, otherwise
    // it returns false.
    bool initializeIfNeeded();

    // A table that consists of misspelled words.
    std::vector<base::string16> m_misspelledWords;

    // A flag representing whether or not this object is initialized.
    bool m_initialized;

    DISALLOW_COPY_AND_ASSIGN(MockSpellCheck);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKSPELLCHECK_H_
