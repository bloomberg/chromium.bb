// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_
#define JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_

#include <string>

struct IncomingNotificationData {
  std::string service_url;
  std::string service_specific_data;
};

struct OutgoingNotificationData {
  OutgoingNotificationData();
  ~OutgoingNotificationData();

  // Id values
  std::string service_url;
  std::string service_id;
  // This bool signifies whether the content fields should be
  // sent with the outgoing data.
  bool send_content;
  // Content values.
  std::string service_specific_data;
  int priority;
  bool require_subscription;
  bool write_to_cache_only;
};

#endif  // JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_

