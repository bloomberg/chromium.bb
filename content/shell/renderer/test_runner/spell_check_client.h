// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELL_CHECK_CLIENT_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "content/shell/renderer/test_runner/mock_spell_check.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

namespace content {

class WebTestDelegate;
class WebTestProxyBase;

class SpellCheckClient : public blink::WebSpellCheckClient {
 public:
  explicit SpellCheckClient(WebTestProxyBase* web_test_proxy);
  virtual ~SpellCheckClient();

  void SetDelegate(WebTestDelegate* delegate);

  WebTaskList* mutable_task_list() { return &task_list_; }
  MockSpellCheck* MockSpellCheckWord() { return &spell_check_; }

  // blink::WebSpellCheckClient implementation.
  virtual void spellCheck(
      const blink::WebString& text,
      int& offset,
      int& length,
      blink::WebVector<blink::WebString>* optional_suggestions);
  virtual void checkTextOfParagraph(
      const blink::WebString& text,
      blink::WebTextCheckingTypeMask mask,
      blink::WebVector<blink::WebTextCheckingResult>* web_results);
  virtual void requestCheckingOfText(
      const blink::WebString& text,
      const blink::WebVector<uint32_t>& markers,
      const blink::WebVector<unsigned>& marker_offsets,
      blink::WebTextCheckingCompletion* completion);
  virtual blink::WebString autoCorrectWord(const blink::WebString& word);

 private:
  void FinishLastTextCheck();

  // The mock spellchecker used in spellCheck().
  MockSpellCheck spell_check_;

  blink::WebString last_requested_text_check_string_;
  blink::WebTextCheckingCompletion* last_requested_text_checking_completion_;

  WebTaskList task_list_;

  WebTestDelegate* delegate_;

  WebTestProxyBase* web_test_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
