// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_

#include "chrome/browser/extensions/api/messaging/message_service.h"

namespace extensions {
class NativeMessageProcessHost;

// A port that manages communication with a native application.
class NativeMessagePort : public MessageService::MessagePort {
 public:
  // Takes ownership of |native_process|.
  explicit NativeMessagePort(NativeMessageProcessHost* native_process);
  virtual ~NativeMessagePort();
  virtual void DispatchOnMessage(const Message& message,
                                 int target_port_id) OVERRIDE;

 private:
  NativeMessageProcessHost* native_process_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
