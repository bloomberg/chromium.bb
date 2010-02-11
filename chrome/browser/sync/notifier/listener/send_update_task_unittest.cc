// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/send_update_task.h"

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

TEST_F(SendUpdateTaskTest, MakeLegacyUpdateMessage) {
  scoped_ptr<buzz::XmlElement> message(
      SendUpdateTask::MakeLegacyUpdateMessage(to_jid_bare_, task_id_));
  const std::string expected_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" data=\"google:notifier\"/>"
                "<ServiceId xmlns=\"\" data=\"notification\"/>"
              "</Id>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

TEST_F(SendUpdateTaskTest, MakeNonLegacyUpdateMessage) {
  scoped_ptr<buzz::XmlElement> new_message(
      SendUpdateTask::MakeNonLegacyUpdateMessage(false, to_jid_bare_,
                                                 task_id_));
  const std::string expected_new_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" "
                            "data=\"http://www.google.com/chrome/sync\"/>"
                "<ServiceId xmlns=\"\" data=\"sync-ping\"/>"
              "</Id>"
              "<Content xmlns=\"\">"
                "<Priority xmlns=\"\" int=\"200\"/>"
                "<RequireSubscription xmlns=\"\" bool=\"true\"/>"
                "<ServiceSpecificData xmlns=\"\" "
                                     "data=\"sync-ping-p2p\"/>"
                "<WriteToCacheOnly xmlns=\"\" bool=\"true\"/>"
              "</Content>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_new_xml_string, XmlElementToString(*new_message));

  scoped_ptr<buzz::XmlElement> transitional_message(
      SendUpdateTask::MakeNonLegacyUpdateMessage(true, to_jid_bare_,
                                                 task_id_));
  const std::string expected_transitional_xml_string =
      StringPrintf(
          "<cli:iq type=\"get\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<set xmlns=\"google:notifier\">"
              "<Id xmlns=\"\">"
                "<ServiceUrl xmlns=\"\" "
                            "data=\"http://www.google.com/chrome/sync\"/>"
                "<ServiceId xmlns=\"\" data=\"sync-ping\"/>"
              "</Id>"
              "<Content xmlns=\"\">"
                "<Priority xmlns=\"\" int=\"200\"/>"
                "<RequireSubscription xmlns=\"\" bool=\"false\"/>"
                "<WriteToCacheOnly xmlns=\"\" bool=\"true\"/>"
              "</Content>"
            "</set>"
          "</cli:iq>",
          to_jid_bare_.Str().c_str(), task_id_.c_str());
  EXPECT_EQ(expected_transitional_xml_string,
            XmlElementToString(*transitional_message));
}

TEST_F(SendUpdateTaskTest, MakeUpdateMessage) {
  scoped_ptr<buzz::XmlElement> expected_legacy_message(
      SendUpdateTask::MakeLegacyUpdateMessage(to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> legacy_message(
      SendUpdateTask::MakeUpdateMessage(NOTIFICATION_LEGACY,
                                        to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_legacy_message),
            XmlElementToString(*legacy_message));

  scoped_ptr<buzz::XmlElement> expected_transitional_message(
      SendUpdateTask::MakeNonLegacyUpdateMessage(true,
                                                 to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> transitional_message(
      SendUpdateTask::MakeUpdateMessage(NOTIFICATION_TRANSITIONAL,
                                        to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_transitional_message),
            XmlElementToString(*transitional_message));

  scoped_ptr<buzz::XmlElement> expected_new_message(
      SendUpdateTask::MakeNonLegacyUpdateMessage(false,
                                                 to_jid_bare_, task_id_));
  scoped_ptr<buzz::XmlElement> new_message(
      SendUpdateTask::MakeUpdateMessage(NOTIFICATION_NEW,
                                        to_jid_bare_, task_id_));
  EXPECT_EQ(XmlElementToString(*expected_new_message),
            XmlElementToString(*new_message));
}

}  // namespace browser_sync
