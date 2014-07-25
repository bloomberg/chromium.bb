// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_

#include <string>

#include "base/basictypes.h"
#include "url/gurl.h"

namespace gcm {

// The prefix used for all push messaging application ids.
extern const char kPushMessagingApplicationIdPrefix[];

// Type used to identify a web app from a Push API perspective.
struct PushMessagingApplicationId {
 public:
  static PushMessagingApplicationId Parse(const std::string& id);

  PushMessagingApplicationId()
      : origin(GURL::EmptyGURL()), service_worker_registration_id(-1) {}
  PushMessagingApplicationId(const GURL& origin,
                             int64 service_worker_registration_id)
      : origin(origin),
        service_worker_registration_id(service_worker_registration_id) {}

  bool IsValid();
  std::string ToString() const;

  const GURL origin;
  const int64 service_worker_registration_id;
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_
