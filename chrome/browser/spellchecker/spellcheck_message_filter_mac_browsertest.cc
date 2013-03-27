// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter_mac.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Fake filter for testing, which stores sent messages and
// allows verification by the test case.
class TestingSpellCheckMessageFilter : public SpellCheckMessageFilterMac {
 public:
  explicit TestingSpellCheckMessageFilter(MessageLoopForUI* loop)
      : SpellCheckMessageFilterMac(),
        loop_(loop) { }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    sent_messages_.push_back(message);
    loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    return true;
  }

  ScopedVector<IPC::Message> sent_messages_;
  MessageLoopForUI* loop_;

 private:
  virtual ~TestingSpellCheckMessageFilter() {}
};

typedef InProcessBrowserTest SpellCheckMessageFilterMacBrowserTest;

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckMessageFilterMacBrowserTest,
                       SpellCheckReturnMessage) {
  scoped_refptr<TestingSpellCheckMessageFilter> target(
      new TestingSpellCheckMessageFilter(MessageLoopForUI::current()));

  SpellCheckHostMsg_RequestTextCheck to_be_received(
      123, 456, UTF8ToUTF16("zz."));
  bool handled = false;
  target->OnMessageReceived(to_be_received, &handled);
  EXPECT_TRUE(handled);

  MessageLoopForUI::current()->Run();
  EXPECT_EQ(1U, target->sent_messages_.size());

  int sent_identifier;
  std::vector<SpellCheckResult> sent_results;
  bool ok = SpellCheckMsg_RespondTextCheck::Read(
      target->sent_messages_[0], &sent_identifier, &sent_results);
  EXPECT_TRUE(ok);
  EXPECT_EQ(1U, sent_results.size());
  EXPECT_EQ(sent_results[0].location, 0);
  EXPECT_EQ(sent_results[0].length, 2);
  EXPECT_EQ(sent_results[0].type,
            SpellCheckResult::SPELLING);
}
