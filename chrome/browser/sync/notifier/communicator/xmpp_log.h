// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_LOG_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_LOG_H_

#if LOGGING

#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"

namespace notifier {

// Log the xmpp input and output.
class XmppLog : public sigslot::has_slots<> {
 public:
  XmppLog() : censor_password_(false) {
  }

  void Input(const char* data, int len) {
    xmpp_input_buffer_.insert(xmpp_input_buffer_.end(), data, data + len);
    XmppPrint(false);
  }

  void Output(const char* data, int len) {
    xmpp_output_buffer_.insert(xmpp_output_buffer_.end(), data, data + len);
    XmppPrint(true);
  }

 private:
  void XmppPrint(bool output);

  std::vector<char> xmpp_input_buffer_;
  std::vector<char> xmpp_output_buffer_;
  bool censor_password_;
  DISALLOW_COPY_AND_ASSIGN(XmppLog);
};

}  // namespace notifier

#endif  // if LOGGING

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_LOG_H_
