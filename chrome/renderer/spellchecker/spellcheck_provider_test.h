// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"

namespace IPC {
  class Message;
}

// A fake completion object for verification.
class FakeTextCheckingCompletion : public WebKit::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion();
  ~FakeTextCheckingCompletion();

  virtual void didFinishCheckingText(
      const WebKit::WebVector<WebKit::WebTextCheckingResult>& results) OVERRIDE;
  virtual void didCancelCheckingText() OVERRIDE;


  size_t completion_count_;
  size_t cancellation_count_;
};

// Faked test target, which stores sent message for verification.
class TestingSpellCheckProvider : public SpellCheckProvider {
 public:
  TestingSpellCheckProvider();

  virtual ~TestingSpellCheckProvider();
  virtual bool Send(IPC::Message* message) OVERRIDE;
  void OnCallSpellingService(int route_id,
                             int identifier,
                             int offset,
                             const string16& text);
  void ResetResult();

  int offset_;
  string16 text_;
  ScopedVector<IPC::Message> messages_;
  size_t spelling_service_call_count_;
};

// SpellCheckProvider test fixture.
class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest();
  virtual ~SpellCheckProviderTest();

 protected:
  TestingSpellCheckProvider provider_;
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_
