// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockSpellCheck.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebCString.h"

using namespace blink;

namespace content {

namespace {

void append(WebVector<WebString>* data, const WebString& item)
{
    WebVector<WebString> result(data->size() + 1);
    for (size_t i = 0; i < data->size(); ++i)
        result[i] = (*data)[i];
    result[data->size()] = item;
    data->swap(result);
}

}

MockSpellCheck::MockSpellCheck()
    : m_initialized(false) { }

MockSpellCheck::~MockSpellCheck() { }

bool MockSpellCheck::spellCheckWord(const WebString& text, int* misspelledOffset, int* misspelledLength)
{
    DCHECK(misspelledOffset);
    DCHECK(misspelledLength);

    // Initialize this spellchecker.
    initializeIfNeeded();

    // Reset the result values as our spellchecker does.
    *misspelledOffset = 0;
    *misspelledLength = 0;

    // Convert to a base::string16 because we store base::string16 instances in
    // m_misspelledWords and WebString has no find().
    base::string16 stringText = text;
    int skippedLength = 0;

    while (!stringText.empty()) {
        // Extract the first possible English word from the given string.
        // The given string may include non-ASCII characters or numbers. So, we
        // should filter out such characters before start looking up our
        // misspelled-word table.
        // (This is a simple version of our SpellCheckWordIterator class.)
        // If the given string doesn't include any ASCII characters, we can treat the
        // string as valid one.
        base::string16::iterator firstChar = std::find_if(stringText.begin(), stringText.end(), isASCIIAlpha);
        if (firstChar == stringText.end())
            return true;
        int wordOffset = std::distance(stringText.begin(), firstChar);
        int maxWordLength = static_cast<int>(stringText.length()) - wordOffset;
        int wordLength;
        base::string16 word;

        // Look up our misspelled-word table to check if the extracted word is a
        // known misspelled word, and return the offset and the length of the
        // extracted word if this word is a known misspelled word.
        // (See the comment in MockSpellCheck::initializeIfNeeded() why we use a
        // misspelled-word table.)
        for (size_t i = 0; i < m_misspelledWords.size(); ++i) {
            wordLength = static_cast<int>(m_misspelledWords.at(i).length()) > maxWordLength ? maxWordLength : static_cast<int>(m_misspelledWords.at(i).length());
            word = stringText.substr(wordOffset, wordLength);
            if (word == m_misspelledWords.at(i) && (static_cast<int>(stringText.length()) == wordOffset + wordLength || isNotASCIIAlpha(stringText[wordOffset + wordLength]))) {
                *misspelledOffset = wordOffset + skippedLength;
                *misspelledLength = wordLength;
                break;
            }
        }

        if (*misspelledLength > 0)
            break;

        base::string16::iterator lastChar = std::find_if(stringText.begin() + wordOffset, stringText.end(), isNotASCIIAlpha);
        if (lastChar == stringText.end())
            wordLength = static_cast<int>(stringText.length()) - wordOffset;
        else
            wordLength = std::distance(firstChar, lastChar);

        DCHECK_LT(0, wordOffset + wordLength);
        stringText = stringText.substr(wordOffset + wordLength);
        skippedLength += wordOffset + wordLength;
    }

    return false;
}

bool MockSpellCheck::hasInCache(const WebString& word)
{
    return word == WebString::fromUTF8("Spell wellcome. Is it broken?") || word == WebString::fromUTF8("Spell wellcome.\x007F");
}

bool MockSpellCheck::isMultiWordMisspelling(const WebString& text, std::vector<WebTextCheckingResult>* results)
{
    if (text == WebString::fromUTF8("Helllo wordl.")) {
        results->push_back(WebTextCheckingResult(WebTextDecorationTypeSpelling, 0, 6, WebString("Hello")));
        results->push_back(WebTextCheckingResult(WebTextDecorationTypeSpelling, 7, 5, WebString("world")));
        return true;
    }
    return false;
}

void MockSpellCheck::fillSuggestionList(const WebString& word, WebVector<WebString>* suggestions)
{
    if (word == WebString::fromUTF8("wellcome"))
        append(suggestions, WebString::fromUTF8("welcome"));
    else if (word == WebString::fromUTF8("upper case"))
        append(suggestions, WebString::fromUTF8("uppercase"));
    else if (word == WebString::fromUTF8("Helllo"))
        append(suggestions, WebString::fromUTF8("Hello"));
    else if (word == WebString::fromUTF8("wordl"))
        append(suggestions, WebString::fromUTF8("world"));
}

bool MockSpellCheck::initializeIfNeeded()
{
    // Exit if we have already initialized this object.
    if (m_initialized)
        return false;

    // Create a table that consists of misspelled words used in WebKit layout
    // tests.
    // Since WebKit layout tests don't have so many misspelled words as
    // well-spelled words, it is easier to compare the given word with misspelled
    // ones than to compare with well-spelled ones.
    static const char* misspelledWords[] = {
        // These words are known misspelled words in webkit tests.
        // If there are other misspelled words in webkit tests, please add them in
        // this array.
        "foo",
        "Foo",
        "baz",
        "fo",
        "LibertyF",
        "chello",
        "xxxtestxxx",
        "XXxxx",
        "Textx",
        "blockquoted",
        "asd",
        "Lorem",
        "Nunc",
        "Curabitur",
        "eu",
        "adlj",
        "adaasj",
        "sdklj",
        "jlkds",
        "jsaada",
        "jlda",
        "zz",
        "contentEditable",
        // The following words are used by unit tests.
        "ifmmp",
        "qwertyuiopasd",
        "qwertyuiopasdf",
        "upper case",
        "wellcome"
    };

    m_misspelledWords.clear();
    for (size_t i = 0; i < arraysize(misspelledWords); ++i)
        m_misspelledWords.push_back(base::string16(misspelledWords[i], misspelledWords[i] + strlen(misspelledWords[i])));

    // Mark as initialized to prevent this object from being initialized twice
    // or more.
    m_initialized = true;

    // Since this MockSpellCheck class doesn't download dictionaries, this
    // function always returns false.
    return false;
}

}  // namespace content
