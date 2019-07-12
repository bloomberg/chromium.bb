// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class AckMessageHandler : public SharingMessageHandler {
 public:
  AckMessageHandler();
  ~AckMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(
      const chrome_browser_sharing::SharingMessage& message) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AckMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_
