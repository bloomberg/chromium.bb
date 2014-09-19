// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/spell_check_client.h"

#include "content/shell/renderer/test_runner/mock_grammar_check.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace content {

namespace {

class HostMethodTask : public WebMethodTask<SpellCheckClient> {
 public:
  typedef void (SpellCheckClient::*CallbackMethodType)();
  HostMethodTask(SpellCheckClient* object, CallbackMethodType callback)
      : WebMethodTask<SpellCheckClient>(object), callback_(callback) {}

  virtual ~HostMethodTask() {}

  virtual void RunIfValid() OVERRIDE { (object_->*callback_)(); }

 private:
  CallbackMethodType callback_;

  DISALLOW_COPY_AND_ASSIGN(HostMethodTask);
};

}  // namespace

SpellCheckClient::SpellCheckClient(WebTestProxyBase* web_test_proxy)
    : last_requested_text_checking_completion_(0),
      web_test_proxy_(web_test_proxy) {
}

SpellCheckClient::~SpellCheckClient() {
}

void SpellCheckClient::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

// blink::WebSpellCheckClient
void SpellCheckClient::spellCheck(
    const blink::WebString& text,
    int& misspelled_offset,
    int& misspelled_length,
    blink::WebVector<blink::WebString>* optional_suggestions) {
  // Check the spelling of the given text.
  spell_check_.SpellCheckWord(text, &misspelled_offset, &misspelled_length);
}

void SpellCheckClient::checkTextOfParagraph(
    const blink::WebString& text,
    blink::WebTextCheckingTypeMask mask,
    blink::WebVector<blink::WebTextCheckingResult>* web_results) {
  std::vector<blink::WebTextCheckingResult> results;
  if (mask & blink::WebTextCheckingTypeSpelling) {
    size_t offset = 0;
    base::string16 data = text;
    while (offset < data.length()) {
      int misspelled_position = 0;
      int misspelled_length = 0;
      spell_check_.SpellCheckWord(
          data.substr(offset), &misspelled_position, &misspelled_length);
      if (!misspelled_length)
        break;
      blink::WebTextCheckingResult result;
      result.decoration = blink::WebTextDecorationTypeSpelling;
      result.location = offset + misspelled_position;
      result.length = misspelled_length;
      results.push_back(result);
      offset += misspelled_position + misspelled_length;
    }
  }
  if (mask & blink::WebTextCheckingTypeGrammar)
    MockGrammarCheck::CheckGrammarOfString(text, &results);
  web_results->assign(results);
}

void SpellCheckClient::requestCheckingOfText(
    const blink::WebString& text,
    const blink::WebVector<uint32_t>& markers,
    const blink::WebVector<unsigned>& marker_offsets,
    blink::WebTextCheckingCompletion* completion) {
  if (text.isEmpty()) {
    if (completion)
      completion->didCancelCheckingText();
    return;
  }

  if (last_requested_text_checking_completion_)
    last_requested_text_checking_completion_->didCancelCheckingText();

  last_requested_text_checking_completion_ = completion;
  last_requested_text_check_string_ = text;
  if (spell_check_.HasInCache(text))
    FinishLastTextCheck();
  else
    delegate_->PostDelayedTask(
        new HostMethodTask(this, &SpellCheckClient::FinishLastTextCheck), 0);
}

void SpellCheckClient::FinishLastTextCheck() {
  if (!last_requested_text_checking_completion_)
    return;
  std::vector<blink::WebTextCheckingResult> results;
  int offset = 0;
  base::string16 text = last_requested_text_check_string_;
  if (!spell_check_.IsMultiWordMisspelling(blink::WebString(text), &results)) {
    while (text.length()) {
      int misspelled_position = 0;
      int misspelled_length = 0;
      spell_check_.SpellCheckWord(
          blink::WebString(text), &misspelled_position, &misspelled_length);
      if (!misspelled_length)
        break;
      blink::WebVector<blink::WebString> suggestions;
      spell_check_.FillSuggestionList(
          blink::WebString(text.substr(misspelled_position, misspelled_length)),
          &suggestions);
      results.push_back(blink::WebTextCheckingResult(
          blink::WebTextDecorationTypeSpelling,
          offset + misspelled_position,
          misspelled_length,
          suggestions.isEmpty() ? blink::WebString() : suggestions[0]));
      text = text.substr(misspelled_position + misspelled_length);
      offset += misspelled_position + misspelled_length;
    }
    MockGrammarCheck::CheckGrammarOfString(last_requested_text_check_string_,
                                           &results);
  }
  last_requested_text_checking_completion_->didFinishCheckingText(results);
  last_requested_text_checking_completion_ = 0;

  web_test_proxy_->PostSpellCheckEvent(blink::WebString("FinishLastTextCheck"));
}

blink::WebString SpellCheckClient::autoCorrectWord(
    const blink::WebString& word) {
  // Returns an empty string as Mac WebKit ('WebKitSupport/WebEditorClient.mm')
  // does. (If this function returns a non-empty string, WebKit replaces the
  // given misspelled string with the result one. This process executes some
  // editor commands and causes layout-test failures.)
  return blink::WebString();
}

}  // namespace content
