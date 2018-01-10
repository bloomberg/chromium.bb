// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_message_filter_platform.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "testing/gtest/include/gtest/gtest.h"

// Fake filter for testing, which stores sent messages and
// allows verification by the test case.
class TestingSpellCheckMessageFilter : public SpellCheckMessageFilterPlatform {
 public:
  explicit TestingSpellCheckMessageFilter(base::OnceClosure&& quit_closure)
      : SpellCheckMessageFilterPlatform(0),
        quit_closure_(std::move(quit_closure)) {}

  bool Send(IPC::Message* message) override {
    sent_messages_.push_back(base::WrapUnique(message));
    main_thread_task_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
    return true;
  }

  std::vector<std::unique_ptr<IPC::Message>> sent_messages_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
  base::OnceClosure quit_closure_;

 private:
  ~TestingSpellCheckMessageFilter() override {}
};

typedef InProcessBrowserTest SpellCheckMessageFilterPlatformMacBrowserTest;

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckMessageFilterPlatformMacBrowserTest,
                       SpellCheckReturnMessage) {
  base::RunLoop run_loop;
  scoped_refptr<TestingSpellCheckMessageFilter> target(
      new TestingSpellCheckMessageFilter(run_loop.QuitWhenIdleClosure()));

  SpellCheckHostMsg_RequestTextCheck to_be_received(123, 456,
                                                    base::UTF8ToUTF16("zz."));
  target->OnMessageReceived(to_be_received);

  run_loop.Run();
  ASSERT_EQ(1U, target->sent_messages_.size());

  SpellCheckMsg_RespondTextCheck::Param params;
  bool ok = SpellCheckMsg_RespondTextCheck::Read(
      target->sent_messages_[0].get(), &params);
  EXPECT_TRUE(ok);

  std::vector<SpellCheckResult> sent_results = std::get<2>(params);
  ASSERT_EQ(1U, sent_results.size());
  EXPECT_EQ(sent_results[0].location, 0);
  EXPECT_EQ(sent_results[0].length, 2);
  EXPECT_EQ(sent_results[0].decoration, SpellCheckResult::SPELLING);
}
