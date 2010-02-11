// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/subscribe_task.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/notifier/listener/xml_element_util.h"
#include "talk/xmpp/jid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace buzz {
class XmlElement;
}

namespace browser_sync {

class SubscribeTaskTest : public testing::Test {
 public:
  SubscribeTaskTest() : to_jid_bare_("to@jid.com"), task_id_("taskid") {
    EXPECT_EQ(to_jid_bare_.Str(), to_jid_bare_.BareJid().Str());
  }

 protected:
  const buzz::Jid to_jid_bare_;
  const std::string task_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SubscribeTaskTest);
};

TEST_F(SubscribeTaskTest, MakeLegacySubscriptionMessage) {
  scoped_ptr<buzz::XmlElement> message(
      SubscribeTask::MakeLegacySubscriptionMessage(to_jid_bare_, task_id_));
  const std::string expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<getAll xmlns=\"google:notifier\">"
              "<ClientActive xmlns=\"\" bool=\"true\"/>"
            "</getAll>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

TEST_F(SubscribeTaskTest, MakeNonLegacySubscriptionMessage) {
  scoped_ptr<buzz::XmlElement> new_message(
      SubscribeTask::MakeNonLegacySubscriptionMessage(false, to_jid_bare_,
                                                      task_id_));
  const std::string expected_new_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<getAll xmlns=\"google:notifier\">"
              "<ClientActive xmlns=\"\" bool=\"true\"/>"
              "<SubscribedServiceUrl "
                  "xmlns=\"\" data=\"http://www.google.com/chrome/sync\"/>"
              "<FilterNonSubscribed xmlns=\"\" bool=\"true\"/>"
            "</getAll>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_new_xml_string, XmlElementToString(*new_message));

  scoped_ptr<buzz::XmlElement> transitional_message(
      SubscribeTask::MakeNonLegacySubscriptionMessage(true, to_jid_bare_,
                                                      task_id_));
  const std::string expected_transitional_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<getAll xmlns=\"google:notifier\">"
              "<ClientActive xmlns=\"\" bool=\"true\"/>"
              "<SubscribedServiceUrl xmlns=\"\" data=\"google:notifier\"/>"
              "<SubscribedServiceUrl "
                  "xmlns=\"\" data=\"http://www.google.com/chrome/sync\"/>"
              "<FilterNonSubscribed xmlns=\"\" bool=\"true\"/>"
            "</getAll>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_transitional_xml_string,
            XmlElementToString(*transitional_message));
}

TEST_F(SubscribeTaskTest, MakeSubscriptionMessage) {
  scoped_ptr<buzz::XmlElement> expected_legacy_message(
      SubscribeTask::MakeLegacySubscriptionMessage(to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> legacy_message(
      SubscribeTask::MakeSubscriptionMessage(NOTIFICATION_LEGACY,
                                             to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_legacy_message),
            XmlElementToString(*legacy_message));

  scoped_ptr<buzz::XmlElement> expected_transitional_message(
      SubscribeTask::MakeNonLegacySubscriptionMessage(true,
                                                      to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> transitional_message(
      SubscribeTask::MakeSubscriptionMessage(NOTIFICATION_TRANSITIONAL,
                                             to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_transitional_message),
            XmlElementToString(*transitional_message));

  scoped_ptr<buzz::XmlElement> expected_new_message(
      SubscribeTask::MakeNonLegacySubscriptionMessage(false,
                                                      to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> new_message(
      SubscribeTask::MakeSubscriptionMessage(NOTIFICATION_NEW,
                                             to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_new_message),
            XmlElementToString(*new_message));
}

}  // namespace browser_sync
