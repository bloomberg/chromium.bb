// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Methods for sending the update stanza to notify peers via xmpp.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_

#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmpptask.h"

namespace browser_sync {

class SendUpdateTask : public buzz::XmppTask {
 public:
  explicit SendUpdateTask(Task* parent);
  virtual ~SendUpdateTask();

  // Overridden from buzz::XmppTask
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon subscription success.
  sigslot::signal1<bool> SignalStatusUpdate;

 private:
  // Allocates and constructs an buzz::XmlElement containing the update stanza.
  buzz::XmlElement* NewUpdateMessage();

  DISALLOW_COPY_AND_ASSIGN(SendUpdateTask);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
