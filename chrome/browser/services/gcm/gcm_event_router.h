// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_EVENT_ROUTER_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_EVENT_ROUTER_H_

#include "base/basictypes.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

// Interface that encapsulates how various GCM events are routed.
class GCMEventRouter {
 public:
  virtual ~GCMEventRouter() {}

  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) = 0;
  virtual void OnMessagesDeleted(const std::string& app_id) = 0;
  virtual void OnSendError(const std::string& app_id,
                           const std::string& message_id,
                           GCMClient::Result result) = 0;
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_EVENT_ROUTER_H_
