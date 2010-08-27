// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/send_update_task.h"

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/jid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace buzz {
class XmlElement;
}

namespace notifier {

class SendUpdateTaskTest : public testing::Test {
 public:
  SendUpdateTaskTest() : to_jid_bare_("to@jid.com"), task_id_("taskid") {
    EXPECT_EQ(to_jid_bare_.Str(), to_jid_bare_.BareJid().Str());
  }

 protected:
  const buzz::Jid to_jid_bare_;
  const std::string task_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendUpdateTaskTest);
};

TEST_F(SendUpdateTaskTest, MakeUpdateMessage) {
  OutgoingNotificationData data;
  data.service_id = "test_service_id";
  data.service_url = "test_service_url";
  data.send_content = false;
  data.priority = 200;
  data.write_to_cache_only = true;
  data.require_subscription = false;

  scoped_ptr<buzz::XmlElement> message_without_content(
      SendUpdateTask::MakeUpdateMessage(data, to_jid_bare_, task_id_));

  std::string expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" data=\"test_service_url\"/>"
                "<ServiceId xmlns=\"\" data=\"test_service_id\"/>"
              "</Id>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_xml_string, XmlElementToString(*message_without_content));

  data.send_content = true;

  expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" "
                            "data=\"test_service_url\"/>"
                "<ServiceId xmlns=\"\" data=\"test_service_id\"/>"
              "</Id>"
              "<Content xmlns=\"\">"
                "<Priority xmlns=\"\" int=\"200\"/>"
                "<RequireSubscription xmlns=\"\" bool=\"false\"/>"
                "<WriteToCacheOnly xmlns=\"\" bool=\"true\"/>"
              "</Content>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());

  scoped_ptr<buzz::XmlElement> message_with_content(
      SendUpdateTask::MakeUpdateMessage(data, to_jid_bare_, task_id_));

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message_with_content));

  data.service_specific_data = "test_service_specific_data";
  data.require_subscription = true;

  expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" "
                            "data=\"test_service_url\"/>"
                "<ServiceId xmlns=\"\" data=\"test_service_id\"/>"
              "</Id>"
              "<Content xmlns=\"\">"
                "<Priority xmlns=\"\" int=\"200\"/>"
                "<RequireSubscription xmlns=\"\" bool=\"true\"/>"
                "<ServiceSpecificData xmlns=\"\" "
                                     "data=\"test_service_specific_data\"/>"
                "<WriteToCacheOnly xmlns=\"\" bool=\"true\"/>"
              "</Content>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());

  scoped_ptr<buzz::XmlElement> message_with_data(
      SendUpdateTask::MakeUpdateMessage(data, to_jid_bare_, task_id_));

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message_with_data));
}

}  // namespace notifier
