// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_

#include <string>

#include "chrome/browser/sync/notification_method.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmpptask.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace browser_sync {

class SendUpdateTask : public buzz::XmppTask {
 public:
  SendUpdateTask(Task* parent, NotificationMethod notification_method);
  virtual ~SendUpdateTask();

  // Overridden from buzz::XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon subscription success.
  sigslot::signal1<bool> SignalStatusUpdate;

 private:
  // Allocates and constructs an buzz::XmlElement containing the update stanza.
  static buzz::XmlElement* MakeUpdateMessage(
      NotificationMethod notification_method,
      const buzz::Jid& to_jid_bare, const std::string& task_id);

  static buzz::XmlElement* MakeLegacyUpdateMessage(
      const buzz::Jid& to_jid_bare, const std::string& task_id);

  static buzz::XmlElement* MakeNonLegacyUpdateMessage(
      bool is_transitional,
      const buzz::Jid& to_jid_bare, const std::string& task_id);

  NotificationMethod notification_method_;

  FRIEND_TEST(SendUpdateTaskTest, MakeUpdateMessage);
  FRIEND_TEST(SendUpdateTaskTest, MakeLegacyUpdateMessage);
  FRIEND_TEST(SendUpdateTaskTest, MakeNonLegacyUpdateMessage);

  DISALLOW_COPY_AND_ASSIGN(SendUpdateTask);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
