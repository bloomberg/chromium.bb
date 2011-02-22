// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/spellcheck_message_filter.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"

namespace {

typedef InProcessBrowserTest SpellCheckMessageFilterBrowserTest;

// Fake filter for testing, which stores sent messages and
// allows verification by the test case.
class TestingSpellCheckMessageFilter : public SpellCheckMessageFilter {
 public:
  explicit TestingSpellCheckMessageFilter(MessageLoopForUI* loop)
      : loop_(loop) { }

  ~TestingSpellCheckMessageFilter() {
    for (std::vector<IPC::Message*>::iterator i = sent_messages_.begin();
         i != sent_messages_.end();
         ++i) {
      delete *i;
    }
  }

  virtual bool Send(IPC::Message* message) {
    sent_messages_.push_back(message);
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    return true;
  }

  std::vector<IPC::Message*> sent_messages_;
  MessageLoopForUI* loop_;
};

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckMessageFilterBrowserTest,
                       SpellCheckReturnMessage) {
  scoped_refptr<TestingSpellCheckMessageFilter> target
      (new TestingSpellCheckMessageFilter(MessageLoopForUI::current()));

  ViewHostMsg_SpellChecker_PlatformRequestTextCheck to_be_received
      (123, 456, 789, UTF8ToUTF16("zz."));
  bool handled = false;
  target->OnMessageReceived(to_be_received, &handled);
  EXPECT_TRUE(handled);

  MessageLoopForUI::current()->Run();
  EXPECT_EQ(1U, target->sent_messages_.size());

  int sent_identifier;
  int sent_tag;
  std::vector<WebKit::WebTextCheckingResult> sent_results;
  bool ok = ViewMsg_SpellChecker_RespondTextCheck::Read(
      target->sent_messages_[0], &sent_identifier, &sent_tag, &sent_results);
  EXPECT_TRUE(ok);
  EXPECT_EQ(1U, sent_results.size());
  EXPECT_EQ(sent_results[0].position(), 0);
  EXPECT_EQ(sent_results[0].length(), 2);
  EXPECT_EQ(sent_results[0].error(),
            WebKit::WebTextCheckingResult::ErrorSpelling);
}

}  // namespace
