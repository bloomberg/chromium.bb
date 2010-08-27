// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/subscribe_task.h"

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/jid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace buzz {
class XmlElement;
}

namespace notifier {

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

TEST_F(SubscribeTaskTest, MakeSubscriptionMessage) {
  std::vector<std::string> subscribed_services_list;

  scoped_ptr<buzz::XmlElement> message_without_services(
      SubscribeTask::MakeSubscriptionMessage(subscribed_services_list,
                                    to_jid_bare_, task_id_));
  std::string expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<getAll xmlns=\"google:notifier\">"
              "<ClientActive xmlns=\"\" bool=\"true\"/>"
            "</getAll>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_xml_string, XmlElementToString(*message_without_services));

  subscribed_services_list.push_back("test_service_url1");
  subscribed_services_list.push_back("test_service_url2");
  scoped_ptr<buzz::XmlElement> message_with_services(
      SubscribeTask::MakeSubscriptionMessage(subscribed_services_list,
                                    to_jid_bare_, task_id_));
  expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<getAll xmlns=\"google:notifier\">"
              "<ClientActive xmlns=\"\" bool=\"true\"/>"
              "<SubscribedServiceUrl "
                  "xmlns=\"\" data=\"test_service_url1\"/>"
              "<SubscribedServiceUrl "
                  "xmlns=\"\" data=\"test_service_url2\"/>"
              "<FilterNonSubscribed xmlns=\"\" bool=\"true\"/>"
            "</getAll>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message_with_services));
}

}  // namespace notifier
