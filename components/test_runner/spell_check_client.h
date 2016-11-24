// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/test_runner/mock_spell_check.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

namespace blink {
class WebTextCheckingCompletion;
}  // namespace blink

namespace test_runner {

class TestRunner;
class WebTestDelegate;

class SpellCheckClient : public blink::WebSpellCheckClient {
 public:
  explicit SpellCheckClient(TestRunner* test_runner);
  virtual ~SpellCheckClient();

  void SetDelegate(WebTestDelegate* delegate);
  void SetEnabled(bool enabled);

  // blink::WebSpellCheckClient implementation.
  void spellCheck(
      const blink::WebString& text,
      int& offset,
      int& length,
      blink::WebVector<blink::WebString>* optional_suggestions) override;
  void requestCheckingOfText(
      const blink::WebString& text,
      const blink::WebVector<uint32_t>& markers,
      const blink::WebVector<unsigned>& marker_offsets,
      blink::WebTextCheckingCompletion* completion) override;
  void cancelAllPendingRequests() override;

 private:
  void FinishLastTextCheck();

  // Do not perform any checking when |enabled_ == false|.
  // Tests related to spell checking should enable it manually.
  bool enabled_ = false;

  // The mock spellchecker used in spellCheck().
  MockSpellCheck spell_check_;

  blink::WebString last_requested_text_check_string_;
  blink::WebTextCheckingCompletion* last_requested_text_checking_completion_;

  TestRunner* test_runner_;
  WebTestDelegate* delegate_;

  base::WeakPtrFactory<SpellCheckClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_SPELL_CHECK_CLIENT_H_
