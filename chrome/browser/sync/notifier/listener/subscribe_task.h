// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class handles subscribing to talk notifications.  It does the getAll
// iq stanza which establishes the endpoint and directs future notifications to
// be pushed.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_

#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmpptask.h"

namespace browser_sync {

class SubscribeTask : public buzz::XmppTask {
 public:
  explicit SubscribeTask(Task* parent);
  virtual ~SubscribeTask();

  // Overridden from XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon subscription success.
  sigslot::signal1<bool> SignalStatusUpdate;

 private:
  // Assembles an Xmpp stanza which can be sent to subscribe to notifications.
  buzz::XmlElement* NewSubscriptionMessage();

  DISALLOW_COPY_AND_ASSIGN(SubscribeTask);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_
