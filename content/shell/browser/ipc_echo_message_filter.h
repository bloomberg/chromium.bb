// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_IPC_ECHO_MESSAGE_FILTER_H_
#define CONTENT_SHELL_BROWSER_IPC_ECHO_MESSAGE_FILTER_H_

#include <string>

#include "content/public/browser/browser_message_filter.h"

namespace content {

class IPCEchoMessageFilter : public BrowserMessageFilter {
 public:
  IPCEchoMessageFilter();

 private:
  virtual ~IPCEchoMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnEchoPing(int routing_id, int id, const std::string& body);

  DISALLOW_COPY_AND_ASSIGN(IPCEchoMessageFilter);
};

}  // namespace content

#endif // CONTENT_SHELL_BROWSER_IPC_ECHO_MESSAGE_FILTER_H_
