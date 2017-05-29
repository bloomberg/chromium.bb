// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace base {
class MessageLoop;
}

namespace IPC {
class Message;
}

// A fake completion object for verification.
class FakeTextCheckingCompletion : public blink::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion();
  ~FakeTextCheckingCompletion();

  void DidFinishCheckingText(
      const blink::WebVector<blink::WebTextCheckingResult>& results) override;
  void DidCancelCheckingText() override;

  size_t completion_count_;
  size_t cancellation_count_;
};

// Faked test target, which stores sent message for verification.
class TestingSpellCheckProvider : public SpellCheckProvider,
                                  public spellcheck::mojom::SpellCheckHost {
 public:
  TestingSpellCheckProvider();
  // Takes ownership of |spellcheck|.
  explicit TestingSpellCheckProvider(SpellCheck* spellcheck);

  ~TestingSpellCheckProvider() override;

  void RequestTextChecking(const base::string16& text,
                           blink::WebTextCheckingCompletion* completion);

  bool Send(IPC::Message* message) override;
  void OnCallSpellingService(const base::string16& text);
  void ResetResult();

  void SetLastResults(
      const base::string16 last_request,
      blink::WebVector<blink::WebTextCheckingResult>& last_results);
  bool SatisfyRequestFromCache(const base::string16& text,
                               blink::WebTextCheckingCompletion* completion);

  base::string16 text_;
  std::vector<std::unique_ptr<IPC::Message>> messages_;
  size_t spelling_service_call_count_;

 private:
  // spellcheck::mojom::SpellCheckerHost:
  void RequestDictionary() override;
  void NotifyChecked(const base::string16& word, bool misspelled) override;
  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override;

  // Message loop (if needed) to deliver the SpellCheckHost request flow.
  std::unique_ptr<base::MessageLoop> loop_;

  // Binding to receive the SpellCheckHost request flow.
  mojo::Binding<spellcheck::mojom::SpellCheckHost> binding_;
};

// SpellCheckProvider test fixture.
class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest();
  ~SpellCheckProviderTest() override;

 protected:
  TestingSpellCheckProvider provider_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
