// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/mailbox.h"

namespace notifier {
TEST_NOTIFIER_F(MailBoxTest);

TEST_F(MailBoxTest, SingleSenderHtml) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(MailSender("Alex Smith", "a@a.com", true, true));
  std::string sender_html = GetSenderHtml(sender_list, 1, me_address, 25);
  ASSERT_STREQ("<b>Alex Smith</b>", sender_html.c_str());
}

TEST_F(MailBoxTest, TruncatedSingleSenderHtml) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(MailSender(
                            "Alex Smith AReallyLongLastNameThatWillBeTruncated",
                            "a@a.com",
                            true,
                            true));
  std::string sender_html = GetSenderHtml(sender_list, 1, me_address, 25);
  ASSERT_STREQ("<b>Alex Smith AReallyLongLa.</b>", sender_html.c_str());
}

TEST_F(MailBoxTest, SingleSenderManyTimesHtml) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(MailSender("Alex Smith", "a@a.com", true, true));
  std::string sender_html = GetSenderHtml(sender_list, 10, me_address, 25);
  ASSERT_STREQ("<b>Alex Smith</b>&nbsp;(10)", sender_html.c_str());
}

TEST_F(MailBoxTest, SenderWithMeHtml) {
  std::string me_address("randOm@comPany.Com");
  MailSenderList sender_list;
  sender_list.push_back(
      MailSender("Alex Smith", "alex@jones.com", false, false));
  sender_list.push_back(
      MailSender("Your Name Goes Here", "raNdom@coMpany.cOm", true, true));
  std::string sender_html = GetSenderHtml(sender_list, 5, me_address, 25);
  ASSERT_STREQ("me,&nbsp;Alex,&nbsp;<b>me</b>&nbsp;(5)", sender_html.c_str());
}

TEST_F(MailBoxTest, SenderHtmlWithAllUnread) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(
      MailSender("Alex Smith", "alex@jones.com", true, false));
  sender_list.push_back(MailSender(
                            "Your Name Goes Here",
                            "foo@company.com",
                            true,
                            true));
  sender_list.push_back(MailSender("", "bob@davis.com", true, false));
  std::string sender_html = GetSenderHtml(sender_list, 100, me_address, 25);
  ASSERT_STREQ("<b>Your</b>,&nbsp;<b>Alex</b>,&nbsp;<b>bob</b>&nbsp;(100)",
               sender_html.c_str());
}

TEST_F(MailBoxTest, SenderHtmlWithTruncatedNames) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(MailSender(
                            "ReallyLongName Smith",
                            "alex@jones.com",
                            true,
                            false));
  sender_list.push_back(MailSender(
                            "AnotherVeryLongFirstNameIsHere",
                            "foo@company.com",
                            true,
                            true));
  std::string sender_html = GetSenderHtml(sender_list, 2, me_address, 25);
  ASSERT_STREQ("<b>AnotherV.</b>,&nbsp;<b>ReallyLo.</b>&nbsp;(2)",
               sender_html.c_str());
}

TEST_F(MailBoxTest, SenderWithTwoSendersShowing) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(
      MailSender("ALongishName Smith", "alex@jones.com", false, false));
  sender_list.push_back(
      MailSender("AnotherBigName", "no@company.com", true, false));
  sender_list.push_back(
      MailSender("Person1", "no1@company.com", true, false));
  sender_list.push_back(
      MailSender("Person2", "no2@company.com", false, true));
  std::string sender_html = GetSenderHtml(sender_list, 6, me_address, 25);
  ASSERT_STREQ("Person2&nbsp;..&nbsp;<b>AnotherB.</b>&nbsp;..&nbsp;(6)",
               sender_html.c_str());
}

TEST_F(MailBoxTest, SenderWithThreeSendersShowing) {
  std::string me_address("random@company.com");
  MailSenderList sender_list;
  sender_list.push_back(
      MailSender("Joe Smith", "alex@jones.com", false, false));
  sender_list.push_back(
      MailSender("Bob Other", "no@company.com", true, false));
  sender_list.push_back(
      MailSender("Person0", "no0@company.com", true, false));
  sender_list.push_back(
      MailSender("Person1", "no1@company.com", true, false));
  sender_list.push_back(
      MailSender("ted", "ted@company.com", false, true));
  std::string sender_html = GetSenderHtml(sender_list, 6, me_address, 25);
  ASSERT_STREQ(
      "ted&nbsp;..&nbsp;<b>Bob</b>&nbsp;..&nbsp;<b>Person1</b>&nbsp;(6)",
      sender_html.c_str());
}
}  // namespace notifier
