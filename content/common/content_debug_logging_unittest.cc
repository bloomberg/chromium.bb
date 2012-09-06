// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_debug_logging.h"

#include <map>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Makes sure that the handlers are cleared during teardown.
class ContentDebugLoggingTest : public testing::Test {
 public:
  ContentDebugLoggingTest() {}
  virtual ~ContentDebugLoggingTest() {}

  void SetUp() {
    messages = &messages_storage;
    content::debug::RegisterMessageHandlers(
        ContentDebugLoggingTest::RecordMsg,
        ContentDebugLoggingTest::GetMessages);
  }

  void TearDown() {
    // Clean up side effects.
    content::debug::RegisterMessageHandlers(NULL, NULL);
    messages = NULL;
  }

  static void RecordMsg(int bug_id, const std::string& msg) {
    if (messages)
      (*messages)[bug_id].push_back(msg);
  }

  static bool GetMessages(int bug_id, std::vector<std::string>* msgs) {
    if (!messages) {
      msgs->clear();
      return false;
    }

    *msgs = (*messages)[bug_id];
    return true;
  }

  // bug_id -> messages for that id.
  typedef std::map<int, std::vector<std::string> > MessageMap;
  MessageMap messages_storage;

  // Points to messages_storage while running tests.
  static MessageMap* messages;
};

// static
ContentDebugLoggingTest::MessageMap* ContentDebugLoggingTest::messages = NULL;

TEST_F(ContentDebugLoggingTest, NullLogging) {
  content::debug::RegisterMessageHandlers(NULL, NULL);

  const std::string kMessage("This is a test");
  const int kBug = 1;
  content::debug::RecordMsg(kBug, kMessage);

  // Cannot get the message back.
  std::vector<std::string> bug_messages;
  EXPECT_FALSE(content::debug::GetMessages(kBug, &bug_messages));
  EXPECT_EQ(bug_messages.size(), 0u);
}

TEST_F(ContentDebugLoggingTest, BasicLogging) {
  const std::string kMessage1("This is a test");
  const std::string kMessage2("That was a test");
  const int kBug = 1;

  // Distinguish ability to retrieve messages from presence of
  // messages.
  std::vector<std::string> bug_messages;
  EXPECT_TRUE(content::debug::GetMessages(kBug, &bug_messages));
  EXPECT_EQ(bug_messages.size(), 0u);

  // Message is stored and can be retrieved.
  content::debug::RecordMsg(kBug, kMessage1);
  EXPECT_TRUE(content::debug::GetMessages(kBug, &bug_messages));
  ASSERT_EQ(bug_messages.size(), 1u);
  EXPECT_EQ(bug_messages[0], kMessage1);

  // Messages are returned in recorded order.
  content::debug::RecordMsg(kBug, kMessage2);
  EXPECT_TRUE(content::debug::GetMessages(kBug, &bug_messages));
  ASSERT_EQ(bug_messages.size(), 2u);
  EXPECT_EQ(bug_messages[0], kMessage1);
  EXPECT_EQ(bug_messages[1], kMessage2);
}

TEST_F(ContentDebugLoggingTest, SeparateBugs) {
  const std::string kMessage("This is a test");
  const int kBug = 1;
  content::debug::RecordMsg(kBug, kMessage);
  content::debug::RecordMsg(17, std::string("Some other message"));

  // Only get the message for this bug back.
  std::vector<std::string> bug_messages;
  EXPECT_TRUE(content::debug::GetMessages(kBug, &bug_messages));
  ASSERT_EQ(bug_messages.size(), 1u);
  EXPECT_EQ(bug_messages[0], kMessage);

  // There are messages for multiple bugs, though.
  EXPECT_EQ(messages->size(), 2u);
}

}  // namespace
