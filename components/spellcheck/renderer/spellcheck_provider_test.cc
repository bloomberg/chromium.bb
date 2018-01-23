// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "ipc/ipc_message.h"

FakeTextCheckingCompletion::FakeTextCheckingCompletion()
    : completion_count_(0), cancellation_count_(0) {}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() {}

void FakeTextCheckingCompletion::DidFinishCheckingText(
    const blink::WebVector<blink::WebTextCheckingResult>& results) {
  ++completion_count_;
}

void FakeTextCheckingCompletion::DidCancelCheckingText() {
  ++completion_count_;
  ++cancellation_count_;
}

TestingSpellCheckProvider::TestingSpellCheckProvider()
    : SpellCheckProvider(nullptr, new SpellCheck(nullptr, nullptr), nullptr),
      spelling_service_call_count_(0),
      binding_(this) {}

TestingSpellCheckProvider::TestingSpellCheckProvider(SpellCheck* spellcheck)
    : SpellCheckProvider(nullptr, spellcheck, nullptr),
      spelling_service_call_count_(0),
      binding_(this) {}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  binding_.Close();
  delete spellcheck_;
}

void TestingSpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (!loop_ && !base::MessageLoop::current())
    loop_ = base::MakeUnique<base::MessageLoop>();
  if (!binding_.is_bound()) {
    spellcheck::mojom::SpellCheckHostPtr host_proxy;
    binding_.Bind(mojo::MakeRequest(&host_proxy));
    SetSpellCheckHostForTesting(std::move(host_proxy));
  }
  SpellCheckProvider::RequestTextChecking(text, completion);
  base::RunLoop().RunUntilIdle();
#else
  SpellCheckProvider::RequestTextChecking(text, completion);
#endif
}

bool TestingSpellCheckProvider::Send(IPC::Message* message) {
  messages_.push_back(base::WrapUnique<IPC::Message>(message));
  return true;
}

void TestingSpellCheckProvider::RequestDictionary() {}

void TestingSpellCheckProvider::NotifyChecked(const base::string16& word,
                                              bool misspelled) {}

void TestingSpellCheckProvider::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  OnCallSpellingService(text);
  std::move(callback).Run(true, std::vector<SpellCheckResult>());
#else
  NOTREACHED();
#endif
}

void TestingSpellCheckProvider::OnCallSpellingService(
    const base::string16& text) {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  ++spelling_service_call_count_;
  blink::WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(last_identifier_);
  if (!completion) {
    ResetResult();
    return;
  }
  text_.assign(text);
  text_check_completions_.Remove(last_identifier_);
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 0, 5,
                                   std::vector<blink::WebString>({"hello"})));
  completion->DidFinishCheckingText(results);
  last_request_ = text;
  last_results_ = results;
#else
  NOTREACHED();
#endif
}

void TestingSpellCheckProvider::ResetResult() {
  text_.clear();
}

void TestingSpellCheckProvider::SetLastResults(
    const base::string16 last_request,
    blink::WebVector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

SpellCheckProviderTest::SpellCheckProviderTest() {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}
