// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_

#include <string>

#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

namespace content {

class PushMessagingMessageFilter : public BrowserMessageFilter {
 public:
  PushMessagingMessageFilter();

 private:
  virtual ~PushMessagingMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnRegister(int routing_id,
                  int callbacks_id,
                  const std::string& sender_id);

  void DidRegister(int routing_id,
                   int callbacks_id,
                   const GURL& endpoint,
                   const std::string& registration_id,
                   bool error);

  DISALLOW_COPY_AND_ASSIGN(PushMessagingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_
