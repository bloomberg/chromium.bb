// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/SpellCheckClient.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/mock_grammar_check.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

using namespace blink;

namespace content {

namespace {

class HostMethodTask : public WebMethodTask<SpellCheckClient> {
public:
    typedef void (SpellCheckClient::*CallbackMethodType)();
    HostMethodTask(SpellCheckClient* object, CallbackMethodType callback)
        : WebMethodTask<SpellCheckClient>(object)
        , m_callback(callback)
    { }

    virtual void runIfValid() OVERRIDE { (m_object->*m_callback)(); }

private:
    CallbackMethodType m_callback;
};

}  // namespace

SpellCheckClient::SpellCheckClient(WebTestProxyBase* webTestProxy)
    : m_lastRequestedTextCheckingCompletion(0)
    , m_webTestProxy(webTestProxy)
{
}

SpellCheckClient::~SpellCheckClient()
{
}

void SpellCheckClient::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

// blink::WebSpellCheckClient
void SpellCheckClient::spellCheck(const WebString& text, int& misspelledOffset, int& misspelledLength, WebVector<WebString>* optionalSuggestions)
{
    // Check the spelling of the given text.
  m_spellcheck.SpellCheckWord(text, &misspelledOffset, &misspelledLength);
}

void SpellCheckClient::checkTextOfParagraph(const WebString& text, WebTextCheckingTypeMask mask, WebVector<WebTextCheckingResult>* webResults)
{
    std::vector<WebTextCheckingResult> results;
    if (mask & WebTextCheckingTypeSpelling) {
        size_t offset = 0;
        base::string16 data = text;
        while (offset < data.length()) {
            int misspelledPosition = 0;
            int misspelledLength = 0;
            m_spellcheck.SpellCheckWord(
                data.substr(offset), &misspelledPosition, &misspelledLength);
            if (!misspelledLength)
                break;
            WebTextCheckingResult result;
            result.decoration = WebTextDecorationTypeSpelling;
            result.location = offset + misspelledPosition;
            result.length = misspelledLength;
            results.push_back(result);
            offset += misspelledPosition + misspelledLength;
        }
    }
    if (mask & WebTextCheckingTypeGrammar)
        MockGrammarCheck::CheckGrammarOfString(text, &results);
    webResults->assign(results);
}

void SpellCheckClient::requestCheckingOfText(
        const WebString& text,
        const WebVector<uint32_t>& markers,
        const WebVector<unsigned>& markerOffsets,
        WebTextCheckingCompletion* completion)
{
    if (text.isEmpty()) {
        if (completion)
            completion->didCancelCheckingText();
        return;
    }

    if (m_lastRequestedTextCheckingCompletion)
        m_lastRequestedTextCheckingCompletion->didCancelCheckingText();

    m_lastRequestedTextCheckingCompletion = completion;
    m_lastRequestedTextCheckString = text;
    if (m_spellcheck.HasInCache(text))
        finishLastTextCheck();
    else
        m_delegate->postDelayedTask(new HostMethodTask(this, &SpellCheckClient::finishLastTextCheck), 0);
}

void SpellCheckClient::finishLastTextCheck()
{
    if (!m_lastRequestedTextCheckingCompletion)
        return;
    std::vector<WebTextCheckingResult> results;
    int offset = 0;
    base::string16 text = m_lastRequestedTextCheckString;
    if (!m_spellcheck.IsMultiWordMisspelling(WebString(text), &results)) {
        while (text.length()) {
            int misspelledPosition = 0;
            int misspelledLength = 0;
            m_spellcheck.SpellCheckWord(
                WebString(text), &misspelledPosition, &misspelledLength);
            if (!misspelledLength)
                break;
            WebVector<WebString> suggestions;
            m_spellcheck.FillSuggestionList(
                WebString(text.substr(misspelledPosition, misspelledLength)),
                &suggestions);
            results.push_back(WebTextCheckingResult(WebTextDecorationTypeSpelling, offset + misspelledPosition, misspelledLength, suggestions.isEmpty() ? WebString() : suggestions[0]));
            text = text.substr(misspelledPosition + misspelledLength);
            offset += misspelledPosition + misspelledLength;
        }
        MockGrammarCheck::CheckGrammarOfString(m_lastRequestedTextCheckString, &results);
    }
    m_lastRequestedTextCheckingCompletion->didFinishCheckingText(results);
    m_lastRequestedTextCheckingCompletion = 0;

    m_webTestProxy->PostSpellCheckEvent(WebString("finishLastTextCheck"));
}

WebString SpellCheckClient::autoCorrectWord(const WebString&)
{
    // Returns an empty string as Mac WebKit ('WebKitSupport/WebEditorClient.mm')
    // does. (If this function returns a non-empty string, WebKit replaces the
    // given misspelled string with the result one. This process executes some
    // editor commands and causes layout-test failures.)
    return WebString();
}

}  // namespace content
