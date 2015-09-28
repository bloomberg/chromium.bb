// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_

#include "base/basictypes.h"
#include "components/test_runner/mock_spell_check.h"
#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

namespace test_runner {

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
  void spellCheck(
      const blink::WebString& text,
      int& offset,
      int& length,
      blink::WebVector<blink::WebString>* optional_suggestions) override;
  void checkTextOfParagraph(
      const blink::WebString& text,
      blink::WebTextCheckingTypeMask mask,
      blink::WebVector<blink::WebTextCheckingResult>* web_results) override;
  void requestCheckingOfText(
      const blink::WebString& text,
      const blink::WebVector<uint32_t>& markers,
      const blink::WebVector<unsigned>& marker_offsets,
      blink::WebTextCheckingCompletion* completion) override;
  blink::WebString autoCorrectWord(const blink::WebString& word) override;

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

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
